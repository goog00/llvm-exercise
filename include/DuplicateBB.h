#ifndef LLVM_EXERCISE_DUPLICATE_BB_H
#define LLVM_EXERCISE_DUPLICATE_BB_H

#include "RIV.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/Pass.h"


#include <map>

struct DuplicateBB : public llvm::PassInfoMixin<DuplicateBB> {

    llvm::PreservedAnalyses run(llvm::Function &F,llvm::FunctionAnalysisManager &);

    //// 将 BB（一个 BasicBlock）映射到 BB 中可访问的一个整数值（在不同的 BasicBlock 中定义）。 
    //克隆 BB 时，在“if-then-else”构造中使用 BB 映射到的值。
    using BBToSingleRIVMap = 
            std::vector<std::tuple<llvm::BasicBlock *,llvm::Value *>>;

    // 将复制前的值映射到 Phi 节点，该节点在复制/克隆后合并相应的值。
    using ValueToPhiMap = std::map<llvm::Value *,llvm::Value *>  ;

    BBToSingleRIVMap  findBBsToDuplicate(llvm::Function &F,const RIV::Result & RIVResult);      


    //赋值bb:
    //使用ContextValue插入’if-then-else'
    //复制BB
    //根据需要添加 PHI 节点
    void cloneBB(llvm::BasicBlock &BB,llvm::Value *ContextValue, ValueToPhiMap &ReMapper);

    unsigned DuplicateBBCount = 0;

    static bool isRequired(){return true;}

};





#endif