#ifndef LLVM_EXERCISE_MBA_SUB_H
#define LLVM_EXERCISE_MBA_SUB_H
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

struct MBASub : public llvm::PassInfoMixin<MBASub> {
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &);

  bool runOnBasicBlock(llvm::BasicBlock &B);

  static bool isRequired() { return true; }
};
#endif