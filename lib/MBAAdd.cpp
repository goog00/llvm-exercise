// a + b == (((a ^ b) + 2 * (a & b)) * 39 + 23) * 151 + 111

#include "MBAAdd.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <random>

using namespace llvm;

#define DEBUG_TYPE "mba-add"

STATISTIC(AddstCount, "The # of substituted instructions");

bool MBAAdd::runOnBasicBlock(BasicBlock &BB) {
  bool Changed = false;

  // 获取一个随机数生成器，用于决定是否替换当前指令。
  // FIXME 我们应该在这里使用“Module::createRNG”。
  // 然而，该方法需要一个指向输入上“Pass”的指针，并且新的pass管理器_do_not_继承自llvm::Pass。
  // 换句话说，这里不能使用“createRNG”，并且没有其他方法可以获取
  // llvm::RandomNumberGenerator。 最后检查 LLVM 8。

  // 使用C++的随机数生成库。你的代码片段创建了一个名为RNG的Mersenne Twister
  // 19937随机数生成器，并使用1234作为种子初始化它。
  // 然后，你定义了一个分布Dist，该分布产生在[0,1)范围内的双精度实数。
  // 如果你想生成一个随机数并使用这个分布，你可以这样做：
  // double random_number = Dist(RNG);
  // 此行代码将从分布Dist中生成一个随机数，并使用Mersenne Twister
  // RNG以1234为种子进行初始化。
  std::mt19937_64 RNG;
  RNG.seed(1234);
  std::uniform_real_distribution<double> Dist(0., 1.);

  for (auto Inst = BB.begin(), IE = BB.end(); Inst != IE; ++Inst) {

    auto *BinOp = dyn_cast<BinaryOperator>(Inst);

    if (!BinOp)
      continue;

    if (BinOp->getOpcode() != Instruction::Add)
      continue;

    // BinOp的类型不是整数类型，或者BinOp的整数位宽不是8
    if (!BinOp->getType()->isIntegerTy() ||
        !(BinOp->getType()->getIntegerBitWidth() == 8)) {
      continue;
    }

    IRBuilder<> Builder(BinOp);

    auto Val39 = ConstantInt::get(BinOp->getType(), 39);
    auto Val151 = ConstantInt::get(BinOp->getType(), 151);
    auto Val23 = ConstantInt::get(BinOp->getType(), 23);
    auto Val2 = ConstantInt::get(BinOp->getType(), 2);
    auto Val111 = ConstantInt::get(BinOp->getType(), 111);

    // a + b == (((a ^ b) + 2 * (a & b)) * 39 + 23) * 151 + 111

    Instruction *NewInst =
        // E = e5 + 111
        BinaryOperator::CreateAdd(
            Val111,
            // e5 = e4 * 151
            Builder.CreateMul(
                Val151,
                // e4 = e2 + 23
                Builder.CreateAdd(
                    Val23,
                    // e3 = e2 * 39
                    Builder.CreateMul(
                        Val39,
                        // e2 = e0 + e1
                        Builder.CreateAdd(
                            // e0 = a ^ b
                            Builder.CreateXor(BinOp->getOperand(0),
                                              BinOp->getOperand(1)),
                            // e1 = 2 * (a & b)
                            Builder.CreateMul(
                                Val2,
                                Builder.CreateAnd(
                                    BinOp->getOperand(0),
                                    BinOp->getOperand(1))))) // e3 = e2 * 39
                    )                                        // e4 = e2 + 23
                )                                            // e5 = e4 * 151
        );                                                   // E = e5 + 111
    // The following is visible only if you pass -debug on the command line
    // *and* you have an assert build.
    LLVM_DEBUG(dbgs() << *BinOp << " -> " << *NewInst << "\n");

    // Replace `(a + b)` (original instructions) with `(((a ^ b) + 2 * (a & b))
    // * 39 + 23) * 151 + 111` (the new instruction)
    ReplaceInstWithInst(&BB, Inst, NewInst);
    Changed = true;

    // Update the statistics
    ++AddstCount;
  }

  return Changed;
}

PreservedAnalyses MBAAdd::run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &) {
  bool Changed = false;

  for (auto &BB : F) {
    Changed |= runOnBasicBlock(BB);
  }
  return (Changed ? llvm::PreservedAnalyses::none()
                  : llvm::PreservedAnalyses::all());
}



//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getMBAAddPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "mba-add", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "mba-add") {
                    FPM.addPass(MBAAdd());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getMBAAddPluginInfo();
}
