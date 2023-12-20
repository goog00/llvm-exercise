//𝑎 = 𝑏 + 1, 𝑐 = 𝑎 − 1 ⇒ 𝑎 = 𝑏 + 1, 𝑐 = 𝑏

#include "llvm/IR/Constant.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
namespace {
class AlgebraicIdentityPass final
    : public PassInfoMixin<AlgebraicIdentityPass> {

public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {

    std::vector<Instruction *> toDelete;

    for (Function &F : M) {
      for (BasicBlock &B : F) {
        for (auto &I : B) {
          //判断指令是否为二元运算
          if (auto *operat = dyn_cast<BinaryOperator>(&I)) {

            auto &op = *operat;

            if (op.getOpcode() == Instruction::Add) {
              //𝑎 = 𝑏 + 1, 𝑐 = 𝑎 − 1 ⇒ 𝑎 = 𝑏 + 1, 𝑐 = 𝑏
              if(ConstantInt *rand = dyn_cast<ConstantInt>(op.getOperand(1))){
                
                }
              }
            } 
          }
        }
      }
    }

    //删除原始的指令
    for (Instruction *I : toDelete) {
      if (I->isSafeToRemove())
        I->eraseFromParent();
    }

    return PreservedAnalyses::all();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "StrengthReductionPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "strength-reduction") {
                    MPM.addPass(AlgebraicIdentityPass());
                    return true;
                  }
                  return false;
                });
          }};
}
