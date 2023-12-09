#ifndef LLVM_EXERCISE_CONVERT_FCMP_EQ_H
#define LLVM_EXERCISE_CONVERT_FCMP_EQ_H

#include "FindFCmpEq.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {
class Function;
}

// New PM interface
struct ConvertFCmpEq : llvm::PassInfoMixin<ConvertFCmpEq> {
  llvm::PreservedAnalyses run(llvm::Function &Func,
                              llvm::FunctionAnalysisManager &FAM);

  bool run(llvm::Function &Func, const FindFCmpEq::Result &Comparison);

  static bool isRequired() { return true; }
};

#endif