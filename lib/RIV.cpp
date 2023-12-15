//=============================================================================
// FILE:
//    RIV.cpp
//
// DESCRIPTION:
//    For every basic block  in the input function, this pass creates a list of
//    integer values reachable from that block. It uses the results of the
//    DominatorTree pass.
//
// ALGORITHM:
//    -------------------------------------------------------------------------
//    v_N = set of integer values defined in basic block N (BB_N)
//    RIV_N = set of reachable integer values for basic block N (BB_N)
//    -------------------------------------------------------------------------
//    STEP 1:
//    For every BB_N in F:
//      compute v_N and store it in DefinedValuesMap
//    -------------------------------------------------------------------------
//    STEP 2:
//    计算入口块 (BB_0) 的 RIV：     
//    Compute the RIVs for the entry block (BB_0):
//      RIV_0 = {input args, global vars}
//    -------------------------------------------------------------------------
//            遍历 CFG，对于 BB_N 占主导地位的每个 BB_M，
//    STEP 3: Traverse the CFG and for every BB_M that BB_N dominates,
//    calculate RIV_M as follows:
//      RIV_M = {RIV_N, v_N}
//    -------------------------------------------------------------------------
//
// REFERENCES:
//    Based on examples from:
//    "Building, Testing and Debugging a Simple out-of-tree LLVM Pass", Serge
//    Guelton and Adrien Guinet, LLVM Dev Meeting 2015
//
// License: MIT
//=============================================================================


//可达整数值：这些值是可以通过在基本块内的操作计算得到的整数值。例如，如果基本块中有 int x = 5; 和 int y = x + 3;，那么可达整数值为 5 和 8。


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
    
  //step2: 计算入口bb,  包括全局变量和输入参数
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