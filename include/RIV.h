//RIV
#ifndef LLVM_EXERCISE_RIV_H
#define LLVM_EXERCISE_RIV_H

#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/Pass.h"

struct RIV : public llvm::AnalysisInfoMixin<RIV> {
    using Result = llvm::MapVector<llvm::BasicBlock const *,llvm::SmallPtrSet<llvm::Value *,8>>;

    Result run(llvm::Function &F, llvm::FunctionAnalysisManager &);

    Result buildRIV(llvm::Function &F,llvm::DomTreeNodeBase<llvm::BasicBlock> * CFGRoot);

    private:
       static llvm::AnalysisKey Key;
       friend struct llvm::AnalysisInfoMixin<RIV>;
};


//new PM interface for the  printer pass
class RIVPrinter : public llvm::PassInfoMixin<RIVPrinter> {

public:
    explicit RIVPrinter(llvm::raw_ostream &OutS) : OS(OutS){}
    llvm::PreservedAnalyses run(llvm::Function &F,
                                llvm::FunctionAnalysisManager &FAM);

    private:
        llvm::raw_ostream &OS;                            
};

#endif