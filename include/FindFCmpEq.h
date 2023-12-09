#ifndef LLVM_EXERCISE_FIND_FCMP_EQ_H
#define LLVM_EXERCISE_FIND_FCMP_EQ_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include <vector>

namespace llvm {
class FCmpInst;
class Function;
class Module;
class raw_ostream;

} // namespace llvm

class FindFCmpEq : public llvm::AnalysisInfoMixin<FindFCmpEq> {
public:
  using Result = std::vector<llvm::FCmpInst *>;

  Result run(llvm::Function &Func, llvm::FunctionAnalysisManager &FAM);

  Result run(llvm::Function &Func);

private:
  friend struct llvm::AnalysisInfoMixin<FindFCmpEq>;
  static llvm::AnalysisKey Key;
};

class FindFCmpEqPrinter : public llvm::PassInfoMixin<FindFCmpEqPrinter> {
public:
  explicit FindFCmpEqPrinter(llvm::raw_ostream &OutStream) : OS(OutStream){};
  llvm::PreservedAnalyses run(llvm::Function &Func,
                              llvm::FunctionAnalysisManager &FAM);

private:
  llvm::raw_ostream &OS;
};

#endif