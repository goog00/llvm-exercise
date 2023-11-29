#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// This method implements what the pass does
void visitor(Function &F) {
  errs() << "(llvm-exercise) hello from " << F.getName() << "\n";
  errs() << "(llvm-exercise)  number of arguments:" << F.arg_size() << "\n";
}

// new PM implementation
struct HelloWorld : PassInfoMixin<HelloWorld> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    visitor(F);
    return PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};
} // namespace

llvm::PassPluginLibraryInfo getHelloWorldPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "helloWorld", LLVM_VERSION_STRING,
          //lamada 函数
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "hello-world") {
                    FPM.addPass(HelloWorld());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo(){
    return getHelloWorldPluginInfo();
}