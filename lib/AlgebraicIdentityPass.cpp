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
              //使用match()检查操作数是否有0 或 1；相似实现也可以用val->getZExtValue() == 0 or 1
              if (PatternMatch::match(op.getOperand(0),
                                      PatternMatch::m_Zero()) ||
                  PatternMatch::match(op.getOperand(1),
                                      PatternMatch::m_Zero())) {
                errs() << "Applying identity x + 0 = x to: " << op << "\n";
                //创建常量：取指令中不为0的操作数，创建常量
                Value *other =
                    op.getOperand(0) == ConstantInt::getNullValue(op.getType())
                        ? op.getOperand(1)
                        : op.getOperand(0);
                //使用常量other ，替换所有使用op指令地方
                op.replaceAllUsesWith(other);
                //op存储到待删除列表
                toDelete.push_back(&op);
              }
            } else if (op.getOpcode() == Instruction::Mul) {
              //x * 1 = x
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
  return {LLVM_PLUGIN_API_VERSION, "AlgebraicIdentityPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
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
