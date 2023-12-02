// 统计编译时函数调用次数
#include "StaticCallCounter.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

static void printStaticCCResult(llvm::raw_ostream &OutS,
                                const ResultStaticCC &DirectCalls);

// StaticCallCounter Implementation
StaticCallCounter::Result StaticCallCounter::runOnModule(Module &M) {
  llvm::MapVector<const llvm::Function *, unsigned> Res;

  for (auto &Func : M) {
    for (auto &BB : Func) {
      for (auto &Ins : BB) {

        // 如果Ins是一个调用指令，CB则不能为空
        auto *CB = dyn_cast<CallBase>(&Ins);
        if (nullptr == CB) {
          continue;
        }
        // 如果CB是直接函数调用，则DirectInvoc不能为空
        auto DirectInvoc = CB->getCalledFunction();
        if (nullptr == DirectInvoc) {
          continue;
        }

        auto CallCount = Res.find(DirectInvoc);
        if (Res.end() == CallCount) {
          CallCount = Res.insert(std::make_pair(DirectInvoc, 0)).first;
        }
        ++CallCount->second;
      }
    }
  }

  return Res;
}

PreservedAnalyses StaticCallCounterPrinter::run(Module &M,
                                                ModuleAnalysisManager &MAM) {
  auto DirectCalls = MAM.getResult<StaticCallCounter>(M);

  printStaticCCResult(OS, DirectCalls);
  return PreservedAnalyses::all();
}

StaticCallCounter::Result
StaticCallCounter::run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
  return runOnModule(M);
}

AnalysisKey StaticCallCounter::Key;

llvm::PassPluginLibraryInfo getStaticCallCounterPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "static-cc", LLVM_VERSION_STRING,
      [](PassBuilder &PB) {
        PB.registerPipelineParsingCallback(
            [&](StringRef Name, ModulePassManager &MPM,
                ArrayRef<PassBuilder::PipelineElement>) {
              if (Name == "print<static-cc>") {
                MPM.addPass(StaticCallCounterPrinter(llvm::errs()));
                return true;
              }
              return false;
            });

        PB.registerAnalysisRegistrationCallback(
          [](ModuleAnalysisManager &MAM) {
          MAM.registerPass([&] { 
            return StaticCallCounter(); 
            });
        });
      }

  };
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getStaticCallCounterPluginInfo();
}

static void printStaticCCResult(raw_ostream &OutS,
                                const ResultStaticCC &DirectCalls) {
  OutS << "=====================\n";

  OutS << "LLVM-EXERCISE: static analysis results";

  OutS << "=====================\n";

  const char *str1 = "NAME";
  const char *str2 = "#N DIRECT CALLS";

  OutS << format("%-20s %-10s\n", str1, str2);

  OutS << "--------------------\n";

  for (auto &CallCount : DirectCalls) {
    OutS << format("%-20s %-10lu\n", CallCount.first->getName().str().c_str(),
                   CallCount.second);
  }

  OutS << "-----------------------------\n\n";
}
