// MergeBB.cpp

#include "MergeBB.h"

#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "MergeBB"

STATISTIC(NumDedupBBs, "Number  of basic blocks merged");
STATISTIC(OverallNumOfUpdatedBranchTargets, "Number of updated branch targets");

// mergeBB implemention

// 检查输入指令Inst（只有一个使用情况）是否可以被移除。如果满足以下条件之一，就可以移除这个指令：
// 指令Inst的唯一使用者是一个PHI节点（因为这种节点可以很容易地更新，即使移除了Inst）。
// 指令Inst的唯一使用者位于与Inst相同的块中（如果该块被移除，那么用户也将被移除）。
// 目的:
// 是为了确定是否可以安全地移除一个指令，同时考虑到了该指令的使用者的情况。
// 如果该指令的使用者是一个PHI节点，那么移除该指令不会影响到其他指令的执行，因为PHI节点会根据需要更新。
// 如果该指令的使用者位于与该指令相同的块中，那么移除该块将自动移除该使用者，因此也可以安全地移除该指令。

// PHI节点是LLVM中的一种特殊指令，用于在多个输入值之间进行选择。它通常用于描述控制块中的值的变化。
// 在LLVM中，当一个指令需要从多个块中进入时，需要使用PHI节点。这些节点可以描述在不同路径上的值。
// 每个PHI节点都有一个入口，对应着一个输入值和一个块。这些入口组成了一个键值对，其中键是块，值是输入值。

bool MergeBB::canRemoveInst(const Instruction *Inst) {
  // dyn_cast是动态类型转换，它用于在运行时根据对象的实际类型来决定转换的类型.
  // 如果转换失败，dyn_cast会返回null。这使得dyn_cast比static_cast更安全，因为它可以防止类型转换错误，并且可以在转换失败时返回一个空指针。
  // cast是静态类型转换，它根据已知的类型来进行转换;这种类型的转换可以在编译时进行，因此比dyn_cast更快。
  // 然而，如果转换失败，可能会产生不可预测的结果，因此在使用cast时需要特别小心

  // 使用assert宏来断言Inst只能被使用一次。
  // 如果这个断言失败（即Inst的使用次数大于1），程序会在运行时抛出一个错误
  assert(Inst->hasOneUse() && "Inst needs to have exactly one use");

  // 使用dyn_cast动态类型检查来尝试将Inst的第一个使用者（通过use_begin()获取）转换为PHINode类型。
  // 如果转换成功，将转换后的对象赋值给指针PNUse。
  auto *PNUse = dyn_cast<PHINode>(*Inst->use_begin());

  // 首先获取Inst所在的父块（通过getParent()获取），然后获取这个父块的终止指令（通过getTerminator()获取），
  // 最后获取这个终止指令的第一个后继块（通过getSuccessor(0)获取），并将这个后继块赋值给指针Succ
  auto *Succ = Inst->getParent()->getTerminator()->getSuccessor(0);

  // 使用cast强制类型转换来尝试将Inst的第一个使用者（通过user_begin()获取）转换为Instruction类型。
  // 如果转换成功，将转换后的对象赋值给指针User。
  auto *User = cast<Instruction>(*Inst->user_begin());

  // 比较User和Inst的父块是否相同
  bool SameParentBB = (User->getParent() == Inst->getParent());
  //
  bool UsedInPhi = (PNUse && PNUse->getParent() == Succ &&
                    PNUse->getIncomingValueForBlock(Inst->getParent()) == Inst);

  return UsedInPhi || SameParentBB;
}

bool MergeBB::canMergeInstructions(ArrayRef<Instruction *> Insts) {

  const Instruction *Inst1 = Insts[0];
  const Instruction *Inst2 = Insts[1];

  if (!Inst1->isSameOperationAs(Inst2))

    return false;

  // Each instruction must have exactly zero or one use.
  bool HasUse = !Inst1->use_empty();
  for (auto *I : Insts) {
    if (HasUse && !I->hasOneUse()) {
      return false;
    }
    if (!HasUse && !I->user_empty()) {
      return false;
    }
  }

  if (HasUse) {
    if (!canRemoveInst(Inst1) || !canRemoveInst(Inst2)) {
      return false;
    }
  }

  // 保证Inst1和Inst2 有相同的操作数
  assert(Inst2->getNumOperands() == Inst1->getNumOperands());
  auto NumOpnds = Inst1->getNumOperands();

  for (unsigned OpndInx = 0; OpndInx != NumOpnds; ++OpndInx) {
    if (Inst2->getOperand(OpndInx) != Inst1->getOperand(OpndInx))
      return false;
  }

  return true;
}

// 在BB中获取非debug 指令的个数
static unsigned getNumNonDbgInstrInBB(BasicBlock *BB) {
  unsigned Count = 0;
  for (Instruction &Instr : *BB) {
    if (!isa<DbgInfoIntrinsic>(Instr))
      Count++;
  }
  return Count;
}

unsigned MergeBB::updateBranchTargets(BasicBlock *BBToErase,
                                      BasicBlock *BBToRetain) {
  SmallVector<BasicBlock *, 8> BBToUpdate(predecessors(BBToErase));

  LLVM_DEBUG(dbgs() << "DEDUP BB: merging duplicated blocks ("
                    << BBToErase->getName() << " into " << BBToRetain->getName()
                    << ")\n");

  unsigned UpdatedTargetsCount = 0;

  for (BasicBlock *BB0 : BBToUpdate) {

    Instruction *Term = BB0->getTerminator();
    for (unsigned OpIdx = 0, NumOpnds = Term->getNumOperands();
         OpIdx != NumOpnds; ++OpIdx) {
      if (Term->getOperand(OpIdx) == BBToErase) {
        Term->setOperand(OpIdx, BBToRetain);
        UpdatedTargetsCount++;
      }
    }
  }

  return UpdatedTargetsCount;
}

bool MergeBB::mergeDuplicatedBlock(BasicBlock *BB1,
                                   SmallPtrSet<BasicBlock *, 8> &DeleteList) {
  if (BB1 == &BB1->getParent()->getEntryBlock()) {
    return false;
  }

  // 只合并无条件分支的CFG边
  BranchInst *BB1Term = dyn_cast<BranchInst>(BB1->getTerminator());
  if (!(BB1Term && BB1Term->isUnconditional())) {
    return false;
  }

  // 非分支和switch的CFG边不用优化
  // predecessors(BB1)函数返回一个包含BB1所有前驱块的迭代器，
  // 然后通过范围for循环，我们可以依次访问这些前驱块，每个前驱块由指针B指向
  for (auto *B : predecessors(BB1)) {
    // B->getTerminator()获取当前前驱块B的终止指令
    // isa<BranchInst>(...)和isa<SwitchInst>(...)是检查指令是否是特定类型的函数。
    // 这里它们分别检查终止指令是否是分支指令（BranchInst）或切换指令（SwitchInst）。
    // 如果终止指令既不是分支指令也不是切换指令，即它是一个非分支或非切换的边缘，那么条件表达式的结果为false，因为逻辑非操作符!将反转结果
    if (!(isa<BranchInst>(B->getTerminator()) ||
          isa<SwitchInst>(B->getTerminator()))) {
      return false;
    }
  }

  BasicBlock *BBSucc = BB1Term->getSuccessor(0);

  BasicBlock::iterator II = BBSucc->begin();
  const PHINode *PN = dyn_cast<PHINode>(II);

  Value *InValBB1 = nullptr;
  Instruction *InInstBB1 = nullptr;
  BBSucc->getFirstNonPHI();

  if (nullptr != PN) {
    // 在后继中如果存在多个PHI节点指令，不用优化（为了简化问题）
    if (++II != BBSucc->end() && isa<PHINode>(II)) {
      return false;
    }

    InValBB1 = PN->getIncomingValueForBlock(BB1);
    InInstBB1 = dyn_cast<Instruction>(InValBB1);
  }

  unsigned BB1NumInst = getNumNonDbgInstrInBB(BB1);
  for (auto *BB2 : predecessors(BBSucc)) {
    // 不优化入口块
    if (BB2 == &BB2->getParent()->getEntryBlock()) {
      continue;
    }

    // 仅合并无条件分支的CFG边
    BranchInst *BB2Term = dyn_cast<BranchInst>(BB2->getTerminator());
    if (!(BB2Term && BB2Term->isUnconditional())) {
      continue;
    }

    for (auto *B : predecessors(BB2)) {
      if (!(isa<BranchInst>(B->getTerminator()) ||
            isa<SwitchInst>(B->getTerminator()))) {
        continue;
      }
    }

    // 跳过已经标记为合并的基本块
    if (DeleteList.end() != DeleteList.find(BB2)) {
      continue;
    }

    // Make sure that BB2 != BB1
    if (BB2 == BB1) {
      continue;
    }

    // 如果指令的数量不相同则BB1和BB2是不一样的
    if (BB1NumInst != getNumNonDbgInstrInBB(BB2)) {
      continue;
    }

    // Control flow can be merged if incoming values to the PHI node
    // at the successor are same values or both defined in the BBs to merge.
    // For the latter case, canMergeInstructions executes further analysis.
    // 如果后继 PHI 节点的传入值 与要合并的 BB 中定义的值相同
    // 或两者都相同，则可以合并控制流。
    if (nullptr != PN) {
      Value *InValBB2 = PN->getIncomingValueForBlock(BB2);
      Instruction *InInstBB2 = dyn_cast<Instruction>(InValBB2);

      bool areValuesSimilar = (InValBB1 == InValBB2);

      bool bothValueDefinedInParent =
          ((InInstBB1 && InInstBB1->getParent() == BB1) &&
           (InInstBB2 && InInstBB2->getParent() == BB2));

      if (!areValuesSimilar && !bothValueDefinedInParent) {
        continue;
      }
    }
  }
}

PreservedAnalyses MergeBB::run(llvm::Function &Func,
                               llvm::FunctionAnalysisManager &) {

  bool Changed = false;
  SmallPtrSet<BasicBlock *, 8> DeleteList;

  for (auto &BB : Func) {
    Changed |= mergeDuplicatedBlock(&BB, DeleteList);
  }
  for (BasicBlock *BB : DeleteList) {
    DeleteDeadBlock(BB);
  }
  return (Changed ? llvm::PreservedAnalyses::none()
                  : llvm::PreservedAnalyses::all());
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getMergeBBPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "MergeBB", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "merge-bb") {
                    FPM.addPass(MergeBB());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getMergeBBPluginInfo();
}

//------------------------------------------------------------------------------
// Helper data structures
//------------------------------------------------------------------------------
LockstepReverseIterator::LockstepReverseIterator(BasicBlock *BB1In,
                                                 BasicBlock *BB2In)
    : BB1(BB1In), BB2(BB2In), Fail(false) {
  Insts.clear();

  Instruction *InstBB1 = getLastNonDbgInst(BB1);
  if (nullptr == InstBB1)
    Fail = true;

  Instruction *InstBB2 = getLastNonDbgInst(BB2);
  if (nullptr == InstBB2)
    Fail = true;

  Insts.push_back(InstBB1);
  Insts.push_back(InstBB2);
}

Instruction *LockstepReverseIterator::getLastNonDbgInst(BasicBlock *BB) {
  Instruction *Inst = BB->getTerminator();

  do {
    Inst = Inst->getPrevNode();
  } while (Inst && isa<DbgInfoIntrinsic>(Inst));

  return Inst;
}

void LockstepReverseIterator::operator--() {
  if (Fail)
    return;

  for (auto *&Inst : Insts) {
    do {
      Inst = Inst->getPrevNode();
    } while (Inst && isa<DbgInfoIntrinsic>(Inst));

    if (!Inst) {
      // Already at the beginning of BB
      Fail = true;
      return;
    }
  }
}
