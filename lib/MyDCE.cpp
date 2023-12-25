//===- DCE.cpp - Code to perform dead code elimination --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements dead inst elimination and dead code elimination.
//
// Dead Inst Elimination performs a single pass over the function removing
// instructions that are obviously dead.  Dead Code Elimination is similar, but
// it rechecks instructions that were used by removed instructions to see if
// they are newly dead.
//
//===----------------------------------------------------------------------===//

#include "MyDCE.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/DebugCounter.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/AssumeBundleBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

#define DEBUG_TYPE "mydce"

STATISTIC(MYDCEEliminated, "Number of insts removed");
DEBUG_COUNTER(MYDCECOUNTER, "mydce-transform",
              "Controls which instructions are eliminated");

//===--------------------------------------------------------------------===//
// RedundantDbgInstElimination pass implementation
//

namespace {

struct RedundantDbgInstElimination : public FunctionPass {
  static char ID;
  RedundantDbgInstElimination() : FunctionPass(ID) {
    initializeRedundantDbgInstEliminationPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override {
    if (skipFunction(F)) {
      return false;
    }

    bool Changed = false;

    for (auto &BB : F) {
      Changed |= RemoveRedundantDbgInstrs(&BB);
    }

    return Changed;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }
};
} // namespace

char RedundantDbgInstElimination::ID = 0;
INITIALIZE_PASS(RedundantDbgInstElimination, "redundant-dbg-inst-elim",
                "Redundant Dbg Instruction Elimination", false, false)

Pass *llvm::createRedundantDbgInstEliminationPass() {
  return new RedundantDbgInstElimination();
}

PreservedAnalyses
RedundantDbgInstEliminationPass::run(Function &F, FunctionAnalysisManager &AM) {
  bool Changed = false;
  for (auto &BB : F) {
    Changed |= RemoveRedundantDbgInstrs(&BB);
  }

  if (!Changed)
    return PreservedAnalyses::all();

  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}

//===--------------------------------------------------------------------===//
// DeadCodeElimination pass implementation
//

// DCEInstruction - 死代码消除的函数实现
// I: 要检查的指令
// WorkList: 存储可能需要进一步消除的指令的工作列表
// TLI: 目标库信息
static bool DCEInstruction(Instruction *I,
                           SmallSetVector<Instruction *, 16> &WorkList,
                           const TargetLibraryInfo *TLI) {
  // 如果指令是无用的（死代码），则进行处理
  if (isInstructionTriviallyDead(I, TLI)) {
    // DebugCounter 检查
    if (!DebugCounter::shouldExecute(MYDCECOUNTER)) {
      return false;
    }

    // 保留与指令相关的调试信息和知识
    salvageDebugInfo(*I);
    salvageKnowledge(I);

    // 遍历指令的所有操作数
    for (unsigned i = 0, e = I->getNumOperands(); i != e; ++i) {
      Value *OpV = I->getOperand(i);
      // 移除操作数，检查其使用情况
      I->setOperand(i, nullptr);
      if (!OpV->use_empty() || I == OpV)
        continue;

      // 如果操作数是无用的指令，则加入工作列表
      if (Instruction *OpI = dyn_cast<Instruction>(OpV))
        if (isInstructionTriviallyDead(OpI, TLI))
          WorkList.insert(OpI);
    }

    // 从其父容器中移除指令
    I->eraseFromParent();
    // 更新消除的指令计数
    ++MYDCEEliminated;
    return true;
  }
  // 如果指令不是无用的，返回 false
  return false;
}



// eliminateDeadCode - 在一个函数中执行死代码消除
// F: 要处理的函数
// TLI: 目标库信息，用于优化
static bool eliminateDeadCode(Function &F, TargetLibraryInfo *TLI) {
    bool MadeChange = false; // 用于跟踪是否进行了任何修改
    SmallSetVector<Instruction *, 16> WorkList; // 保存待处理的指令集合

    // 遍历函数中的所有指令
    for (Instruction &I : llvm::make_early_inc_range(instructions(F))) {
        // 如果指令不在工作列表中，检查是否为死代码
        if (!WorkList.count(&I)) 
          MadeChange |= DCEInstruction(&I, WorkList, TLI);
    }

    // 处理工作列表中的指令，直到列表为空
    while (!WorkList.empty()) {
        Instruction *I = WorkList.pop_back_val();
        MadeChange |= DCEInstruction(I, WorkList, TLI);
    }

    return MadeChange; // 返回是否进行了修改
}

// MyDCEPass::run - LLVM优化通行的运行方法
// F: 要处理的函数
// AM: 函数分析管理器
PreservedAnalyses MyDCEPass::run(Function &F, FunctionAnalysisManager &AM) {
    // 执行死代码消除
    if (!eliminateDeadCode(F, &AM.getResult<TargetLibraryAnalysis>(F)))
        return PreservedAnalyses::all(); // 如果没有修改，保留所有分析结果

    PreservedAnalyses PA; // 创建保留分析结果的对象
    PA.preserveSet<CFGAnalyses>(); // 保留控制流图分析结果
    return PA; // 返回保留的分析结果
}



namespace {
struct DCELegacyPass : public FunctionPass {



};   
}