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

            if (op.getOpcode() == Instruction::Mul) {
              
              // x * 2 ===> x<<1;
              if(ConstantInt *rand = dyn_cast<ConstantInt>(op.getOperand(1))){
                if(rand->getZExtValue() == 2){
                  errs() << "Applying identity x * 2 ==>  x<<1 to: " << op << "\n";
                  IRBuilder<> builder(M.getContext());
                  //表示新创建的指令将被放置在op所在的基本块中，并且位于op之前
                  builder.SetInsertPoint(&op);
                  //创建左移指令：参数1是第一个操作数，参数2是1
                  Value *shiftLeft =  builder.CreateShl(op.getOperand(0),ConstantInt::get(op.getType(),1));
                  errs() << "shiftLeft: " <<  *shiftLeft;
                  op.replaceAllUsesWith(shiftLeft);
                  toDelete.push_back(&op);
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
