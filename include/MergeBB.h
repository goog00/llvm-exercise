// MergeBB.h

#ifndef LLVM_EXERCISE_MERGEBBS_H
#define LLVM_EXERCISE_MERGEBBS_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

using ResultMergeBB = llvm::StringMap<unsigned>;

struct MergeBB : public llvm::PassInfoMixin<MergeBB> {
  using Result = ResultMergeBB;
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &);

// 检查输入指令Inst（只有一个使用情况）是否可以被移除。如果满足以下条件之一，就可以移除这个指令：
// 指令Inst的唯一使用者是一个PHI节点（因为这种节点可以很容易地更新，即使移除了Inst）。
// 指令Inst的唯一使用者位于与Inst相同的块中（如果该块被移除，那么用户也将被移除）。
// 目的: 是为了确定是否可以安全地移除一个指令，同时考虑到了该指令的使用者的情况。
//如果该指令的使用者是一个PHI节点，那么移除该指令不会影响到其他指令的执行，因为PHI节点会根据需要更新。
//如果该指令的使用者位于与该指令相同的块中，那么移除该块将自动移除该使用者，因此也可以安全地移除该指令。

//PHI节点是LLVM中的一种特殊指令，用于在多个输入值之间进行选择。它通常用于描述控制块中的值的变化。
//在LLVM中，当一个指令需要从多个块中进入时，需要使用PHI节点。这些节点可以描述在不同路径上的值。
//每个PHI节点都有一个入口，对应着一个输入值和一个块。这些入口组成了一个键值对，其中键是块，值是输入值。
  bool canRemoveInst(const llvm::Instruction *Inst);

  // Insts中的指令属于不同的块，这些块是无条件跳转到共同的继承者。
  // 分析他们，如果要合并他们就返回true
  bool canMergeInstructions(llvm::ArrayRef<llvm::Instruction *> Insts);

  // 通过BBToRetain替换BBToErase的入边的目的地
  unsigned updateBranchTargets(llvm::BasicBlock *BBToErase,
                               llvm::BasicBlock *BBToRetain);

  // 如果BB是复制的，然后使用他的复制对BB做合并，把BB添加到删除列表。删除列表包含要被删除的blocks
  bool
  mergeDuplicatedBlock(llvm::BasicBlock *BB,
                       llvm::SmallPtrSet<llvm::BasicBlock *, 8> &DeleteList);

  static bool isRequired() { return true; }
};

class LockstepReverseIterator {
      llvm::BasicBlock *BB1;
      llvm::BasicBlock *BB2;
      
      llvm::SmallVector<llvm::Instruction *,2> Insts;

      bool Fail;

public:
    LockstepReverseIterator(llvm::BasicBlock *BB1In,llvm::BasicBlock *BB2In);

    llvm::Instruction *getLastNonDbgInst(llvm::BasicBlock *BB);
    bool isValid() const {return !Fail;}

    void operator--();

    llvm::ArrayRef<llvm::Instruction *> operator*() {return Insts;}      
};


#endif
