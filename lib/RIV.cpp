//=======================================================
//FIFE:
//     RIV.cpp
//DESCRIPTION:
//      对于输入函数中的每个基本块，此Pass都会创建一个可从该块访问的整数值列表。 
//      它使用 DominatorTree 传递的结果。
//ALGORITHM:
//      



#include "RIV.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Format.h"

#include <deque>

using namespace llvm;

using NodeTy = DomTreeNodeBase<llvm::BasicBlock> *;

using DefValMapTy = RIV::Result;

static void printRIVResult(llvm::raw_ostream &OutS,const RIV::Result &RIVMap);

RIV::Result RIV::buildRIV(Function &F,NodeTy CFGRoot) {

    Result ResultMap;

    //初始化双端队列，保存CFG中结点
    std::deque<NodeTy> BBsToProcess;
    //CFGRoot 添加到双端队列的尾部
    BBsToProcess.push_back(CFGRoot);

    DefValMapTy DefinedValuesMap;

    for(BasicBlock &BB : F) {
        auto &Values = DefinedValuesMap[&BB];
        for(Instruction &Inst : BB){
            if(Inst.getType()->isIntegerTy()){
                Values.insert(&Inst);
            }
        }

    }
    
  //step2: 为BB计算 RIVS,包括全局变量和输入参数
   auto &EntryBBValues = ResultMap[&F.getEntryBlock()];

   for(auto &Global : F.getParent()->globals()) {
       if(Global.getValueType()->isIntegerTy()){
          EntryBBValues.insert(&Global);
       }
   }

   for(Argument &Arg : F.args()) {
      if(Arg.getType()->isIntegerTy()){
        EntryBBValues.insert(&Arg);
      }
   }

  //step3: 遍历F中每个BB的CFG计算它的RIVs
  //遍历双端队列，
  while (!BBsToProcess.empty()){
        //第一次循环，取出CFGRoot
        auto *Parent =  BBsToProcess.back();
        BBsToProcess.pop_back();

        auto &ParentDefs =  DefinedValuesMap[Parent->getBlock()];
        
        llvm::SmallPtrSet<llvm::Value *,8> ParentRIVS = ResultMap[Parent->getBlock()];
        
        for(NodeTy Child : *Parent){
            BBsToProcess.push_back(Child);
            auto ChildBB = Child->getBlock();

            ResultMap[ChildBB].insert(ParentDefs.begin(),ParentDefs.end());


            ResultMap[ChildBB].insert(ParentRIVS.begin(),ParentRIVS.end());

        }

  }


  return ResultMap;
          
}

RIV::Result RIV::run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM) {
  DominatorTree *DT = &FAM.getResult<DominatorTreeAnalysis>(F);
  Result Res = buildRIV(F, DT->getRootNode());

  return Res;
}

PreservedAnalyses RIVPrinter::run(Function &Func,
                                  FunctionAnalysisManager &FAM) {

  auto RIVMap = FAM.getResult<RIV>(Func);

  printRIVResult(OS, RIVMap);
  return PreservedAnalyses::all();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
AnalysisKey RIV::Key;

llvm::PassPluginLibraryInfo getRIVPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "riv", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            // #1 REGISTRATION FOR "opt -passes=print<riv>"
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "print<riv>") {
                    FPM.addPass(RIVPrinter(llvm::errs()));
                    return true;
                  }
                  return false;
                });
            // #2 REGISTRATION FOR "FAM.getResult<RIV>(Function)"
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([&] { return RIV(); });
                });
          }};
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getRIVPluginInfo();
}

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------
static void printRIVResult(raw_ostream &OutS, const RIV::Result &RIVMap) {
  OutS << "=================================================\n";
  OutS << "LLVM-TUTOR: RIV analysis results\n";
  OutS << "=================================================\n";

  const char *Str1 = "BB id";
  const char *Str2 = "Reachable Ineger Values";
  OutS << format("%-10s %-30s\n", Str1, Str2);
  OutS << "-------------------------------------------------\n";

  const char *EmptyStr = "";

  for (auto const &KV : RIVMap) {
    std::string DummyStr;
    raw_string_ostream BBIdStream(DummyStr);
    KV.first->printAsOperand(BBIdStream, false);
    OutS << format("BB %-12s %-30s\n", BBIdStream.str().c_str(), EmptyStr);
    for (auto const *IntegerValue : KV.second) {
      std::string DummyStr;
      raw_string_ostream InstrStr(DummyStr);
      IntegerValue->print(InstrStr);
      OutS << format("%-12s %-30s\n", EmptyStr, InstrStr.str().c_str());
    }
  }

  OutS << "\n\n";
}