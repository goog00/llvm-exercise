#ifndef LLVM_EXERCISE_INSTRUMENT_BASIC_H
#define LLVM_EXERCISE_INSTRUMENT_BASIC_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

//New PM Interface
struct InjectFuncCall : public llvm::PassInfoMixin<InjectFuncCall> {
    
    llvm::PreservedAnalyses run(llvm::Module &M,
                                llvm::ModuleAnalysisManager &);

    bool runOnModule(llvm::Module &M);


    static bool isRequired(){return true;}                            
};

#endif