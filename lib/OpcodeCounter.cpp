#include "OpcodeCounter.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

static void printOpcodeCounterResult(llvm::raw_ostream &,
                                    const ResultOpcodeCounter &OC);


//OpcodeCounter implementation

// llvm::AnalysisKey OpcodeCounter::Key; 这行代码定义了一个名为Key的OpcodeCounter类的成员变量，类型为llvm::AnalysisKey。
// llvm::AnalysisKey是LLVM库中用于表示一种特定的分析的键值。这种键值通常用于在分析注册和执行的过程中识别和查找特定的分析。
// 在这行代码中，Key被定义为OpcodeCounter类的一个静态成员变量，用于标识和查找与OpcodeCounter相关的特定分析。
llvm::AnalysisKey OpcodeCounter::Key;

OpcodeCounter::Result OpcodeCounter::generateOpcodeMap(llvm::Function &Func) {
    OpcodeCounter::Result OpcodeMap;

    for(auto &BB : Func) {
        for(auto &Inst : BB) {
            StringRef Name = Inst.getOpcodeName();

            if(OpcodeMap.find(Name) == OpcodeMap.end()) {
                OpcodeMap[Inst.getOpcodeName()] = 1;
            } else {
                OpcodeMap[Inst.getOpcodeName()] ++;
            }
        }
    }

    return OpcodeMap;
}

OpcodeCounter::Result OpcodeCounter::run(llvm::Function &Func,llvm::FunctionAnalysisManager &) {
    return generateOpcodeMap(Func);
}


PreservedAnalyses OpcodeCounterPrinter::run(Function &Func,
                                            FunctionAnalysisManager &FAM) {
        auto &OpcodeMap = FAM.getResult<OpcodeCounter>(Func);

        OS << "Printing analysis 'OpcodeCounter Pass' for function '"
           << Func.getName() << "':\n";

        printOpcodeCounterResult(OS,OpcodeMap);   
        return PreservedAnalyses::all();
                                            
}


//New PM Registeration

llvm::PassPluginLibraryInfo getOpcodeCounterPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,"OpcodeCounter",LLVM_VERSION_STRING,
        [](PassBuilder &PB){
            // #1 REGISTRATION FOR "opt -passes=print<opcode-counter>"
          // Register OpcodeCounterPrinter so that it can be used when
          // specifying pass pipelines with `-passes=`.
          PB.registerPipelineParsingCallback([&](StringRef Name,FunctionPassManager &FPM,
                ArrayRef<PassBuilder::PipelineElement>){
                    if(Name == "print<opcode-counter>") {
                        FPM.addPass(OpcodeCounterPrinter(llvm::errs()));
                        return true;
                    }
                    return false;

          });

          PB.registerVectorizerStartEPCallback(
            [](llvm::FunctionPassManager &PM,
               llvm::OptimizationLevel Level) {
                  PM.addPass(OpcodeCounterPrinter(llvm::errs()));
               }
            );

           PB.registerAnalysisRegistrationCallback(
            [](FunctionAnalysisManager &FAM){
                FAM.registerPass([&]{return OpcodeCounter();});
            }
           );
          
        }
    };
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo(){
    return getOpcodeCounterPluginInfo();
}


static void printOpcodeCounterResult(raw_ostream &OutS, const ResultOpcodeCounter &OpcodeMap) {
    OutS << "=================="
    << "\n";

    OutS << "LLVM-EXERCISE: OpcodeCounter results\n";
    OutS << "========================================\n";
    const char *str1 = "OPCODE";
    const char *str2 = "#TIME USED";
    OutS << format("%-20s %-10s\n", str1, str2);
    OutS << "-------------------------------\n";

     for(auto &Inst : OpcodeMap) {
        OutS << format("%-20s %-10lu\n",Inst.first().str().c_str(),Inst.second);

     }
     OutS << "----------------------------------\n\n";

}