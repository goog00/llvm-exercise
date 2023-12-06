//==================================
//FIFE: MBAADD.h
#ifndef LLVM_EXERCISE_MBA_ADD_H
#define LLVM_EXERCISE_MBA_ADD_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

//New PM interface
struct MBAAdd : public llvm::PassInfoMixin<MBAAdd> {
    llvm::PreservedAnalyses  run(llvm::Function &F,llvm::FunctionAnalysisManager &);

    bool runOnBasicBlock(llvm::BasicBlock &B);

    static bool isRequired(){return true;}
};

#endif