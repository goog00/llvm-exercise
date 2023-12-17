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

          if (auto *operator1 = dyn_cast<BinaryOperator>(&I)) {

            auto &op = *operator1;

            if (op.getOpcode() == Instruction::Add) {
              if (PatternMatch::match(op.getOperand(0),
                                      PatternMatch::m_Zero()) ||
                  PatternMatch::match(op.getOperand(1),
                                      PatternMatch::m_Zero())) {
                errs() << "Applying identity x + 0 = x to: " << op << "\n";
                Value *other =
                    op.getOperand(0) == ConstantInt::getNullValue(op.getType())
                        ? op.getOperand(1)
                        : op.getOperand(0);
                op.replaceAllUsesWith(other);

                toDelete.push_back(&op);
              }
            } else if (op.getOpcode() == Instruction::Mul) {
              if (PatternMatch::match(op.getOperand(0),
                                      PatternMatch::m_One()) ||
                  PatternMatch::match(op.getOperand(1),
                                      PatternMatch::m_One())) {
                errs() << "Applying identity x * 1 = x to: " << op << "\n";
                Value *other = ConstantInt::get(op.getType(), 1)
                                   ? op.getOperand(1)
                                   : op.getOperand(0);

                op.replaceAllUsesWith(other);
                toDelete.push_back(&op);
              }
            }
          }
        }
      }
    }

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
  return {LLVM_PLUGIN_API_VERSION, "AlgebraicIdentityPass",
          LLVM_VERSION_STRING, [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "algebraic-identity") {
                    MPM.addPass(AlgebraicIdentityPass());
                    return true;
                  }
                  return false;
                });
          }};
}
