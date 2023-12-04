//==========================================================
// FILE:
//      MBA.cpp
//
// DESCRIPTION:
//    使用公式：a - b == (a + ~b) + 1 对减法运算优化;
//    公式含义：对于任何整数a和b，表达式a - b等于表达式(a + ~b) + 1。
//    其中，~b是对b进行按位取反操作，即把b中的每个位都取反。
//
//
//===========================================================

#include "MBASub.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <random>

using namespace llvm;

#define DEBUG_TYPE "mba-sub"

// 当你使用 STATISTIC(SubstCount,"The # of substituted instructions");
// 这样的语句时，预处理器会将其替换为 static llvm::Statistic SubstCount =
// {DEBUG_TYPE, "SubstCount", "The # of substituted instructions"};。
//  Update the statistics   ++SubstCount;
STATISTIC(SubstCount, "The # of substituted instructions");

// MBASub implementation
bool MBASub::runOnBasicBlock(BasicBlock &BB) {
  bool Changed = false;

  for (auto Inst = BB.begin(), IE = BB.end(); Inst != IE; ++Inst) {

    // 跳过非二元的指令（e.g. unary or compare ）
    auto *BinOp = dyn_cast<BinaryOperator>(Inst);
    if (!BinOp)
      continue;

    // 跳过整数 sub 以外的指令。
    unsigned Opcode = BinOp->getOpcode();
    if (Opcode != Instruction::Sub || !BinOp->getType()->isIntegerTy())
      continue;

    // 用于创建指令并将其插入基本块的统一 API。
    IRBuilder<> Builder(BinOp);

    // 创建一个指令表示 (a + ~b) + 1
    Instruction *NewValue = BinaryOperator::CreateAdd(
        Builder.CreateAdd(BinOp->getOperand(0),
                          Builder.CreateNot(BinOp->getOperand(1))),
        ConstantInt::get(BinOp->getType(), 1));

    LLVM_DEBUG(dbgs() << *BinOp << " -> " << *NewValue << "\n");

    // 使用’（a + ~b) + 1' 替换 '(a - b)'
    ReplaceInstWithInst(&BB, Inst, NewValue);

    Changed = true;

    ++SubstCount;
  }

  return Changed;
}

// PreservedAnalyses是LLVM库中一个枚举类型，用于表示函数分析结果是否被保留。它有两个可能的值：
// llvm::PreservedAnalyses::all():
// 表示保留所有的分析结果。这意味着在执行某些操作或变换后，函数的分析结果不会被改变。
// llvm::PreservedAnalyses::none():
// 表示不保留任何分析结果。这意味着在执行某些操作或变换后，函数的分析结果会被完全改变或失效。
// PreservedAnalyses通常用于函数分析器的上下文中，用于指示函数分析结果是否在执行某些操作或变换后仍然有效。它可以帮助优化函数分析的过程，避免对已经失效的分析结果进行不必要的计算。
// 在你的代码中，run函数返回一个PreservedAnalyses枚举值，根据Changed变量的值来决定返回哪个值。如果Changed为true，表示函数的分析结果已经被改变，因此返回PreservedAnalyses::none()表示不保留任何分析结果。
// 如果Changed为false，表示函数的分析结果没有被改变，因此返回PreservedAnalyses::all()表示保留所有的分析结果。

PreservedAnalyses MBASub::run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &) {
  bool Changed = false;

  for (auto &BB : F) {
    // 在C++中，|=是一种位运算符，称为“位或赋值运算符”。它将左操作数和右操作数的每个位进行“或”运算，然后将结果赋值给左操作数。
    // 具体来说，对于两个二进制数A和B，A |= B等同于A = A |
    // B，其中|表示位或运算。
    Changed |= runOnBasicBlock(BB);
  }

  return (Changed ? llvm::PreservedAnalyses::none()
                  : llvm::PreservedAnalyses::all());
}

llvm::PassPluginLibraryInfo getMBASubPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "mba-sub", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "mba-sub") {
                    FPM.addPass(MBASub());
                    return true;
                  }
                  return false;
                });
          }

  };
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getMBASubPluginInfo();
}