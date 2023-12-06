//==============================================================================
//  FILE:
//    DuplicateBB.cpp
//

#include "DuplicateBB.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <random>

#define DEBUG_TYPE "duplicate-bb"

STATISTIC(DuplicateBBCountStats, "The # of duplicate blocks");

using namespace llvm;

DuplicateBB::BBToSingleRIVMap
DuplicateBB::findBBsToDuplicate(Function &F, const RIV::Result &RIVResult) {
  BBToSingleRIVMap BlocksToDuplicate;

  // 用于需要高质量（“真正的”）随机数
  std::random_device RD;
  // 这行代码声明并初始化了一个名为RNG的std::mt19937_64对象。
  // 这个对象是一个Mersenne
  // Twister随机数生成器，它使用std::random_device生成的随机数作为种子。
  // 这样，生成的随机数序列将具有高质量的统计特性。
  std::mt19937_64 RNG(RD());

  for (BasicBlock &BB : F) {
    //
    // 基本块中“landing pad” 不在复制的范围
    // “landing
    // pad”通常是指异常处理代码的一部分。当程序发生异常时，控制流会“跳转”到这个landing
    // pad。
    if (BB.isLandingPad()) {
      continue;
    }
    // 获取BB的RIVS集合
    auto const &ReachableValues = RIVResult.lookup(&BB);
    size_t ReachableValuesCount = ReachableValues.size();

    if (0 == ReachableValuesCount) {
      LLVM_DEBUG(errs() << "No context values for this BB\n");
      continue;
    }

    // 从RIV set 中获取一个随机上下文值
    auto Iter = ReachableValues.begin();
    // 产生均匀分布的随机数生成器uniform_int_distribution，ReachableValuesCount
    // 是一个定义了可达到值的整数。
    std::uniform_int_distribution<> Dist(0, ReachableValuesCount - 1);
    // std::advance函数来移动迭代器
    std::advance(Iter, Dist(RNG));

    if (dyn_cast<GlobalValue>(*Iter)) {
      LLVM_DEBUG(errs() << "Random  context value is a global variable ."
                        << "Skipping this BB\n");
      continue;
    }

    LLVM_DEBUG(errs() << "Random context value :" << **Iter << "\n");

    BlocksToDuplicate.emplace_back(&BB, *Iter);
  }

  return BlocksToDuplicate;
}

void DuplicateBB::cloneBB(BasicBlock &BB, Value *ContextValue,
                          ValueToPhiMap &ReMapper) {
  // 不要重复 Phi 节点 - 从它们之后开始
  Instruction *BBHead = BB.getFirstNonPHI();

  // 为'if-then-else'创建条件
  IRBuilder<> Builder(BBHead);
  Value *Cond = Builder.CreateIsNull(
      ReMapper.count(ContextValue) ? ReMapper[ContextValue] : ContextValue);

  Instruction *ThenTerm = nullptr;
  Instruction *ElseTerm = nullptr;

  SplitBlockAndInsertIfThenElse(Cond, &*BBHead, &ThenTerm, &ElseTerm);

  BasicBlock *Tail = ThenTerm->getSuccessor(0);

  assert(Tail == ElseTerm->getSuccessor(0) && "Inconsistent CFG");

  // 给新BB起一个有意义的名字
  std::string DuplicateBBId = std::to_string(DuplicateBBCount);
  ThenTerm->getParent()->setName("lt-clone-1-" + DuplicateBBId);
  ThenTerm->getParent()->setName("lt-clone-2-" + DuplicateBBId);

  Tail->setName("lt-tail-" + DuplicateBBId);

  ThenTerm->getParent()->getSinglePredecessor()->setName("lt-if-then-else-" +
                                                         DuplicateBBId);

  // 用于跟踪新绑定的变量
  ValueToValueMapTy TailVMap, ThenVMap, ElseVMap;

  // 尾部不产生任何数据的指令集 可以remove
  SmallVector<Instruction *, 8> ToRemove;

  // 迭代原始指令复制每个指令到if-then 和 else 分支，更新绑定/使用
  // 在此阶段，除了 PHI 节点之外的所有指令都存储在 Tail 中。
  for (auto IIT = Tail->begin(), IE = Tail->end(); IIT != IE; ++IIT) {
    Instruction &Instr = *IIT;
    assert(!isa<PHINode>(&Instr) && "phi nodes have already been filtered out");

    if (Instr.isTerminator()) {
      RemapInstruction(&Instr, TailVMap, RF_IgnoreMissingLocals);
    }

    Instruction *ThenClone = Instr.clone(), *ElseClone = Instr.clone();

    // ThenClone 的操作数仍然保留对原始 BB 的引用。
    // 更新/重新映射它们。
    RemapInstruction(ThenClone, ThenVMap, RF_IgnoreMissingLocals);
    ThenClone->insertBefore(ThenTerm);
    ThenVMap[&Instr] = ThenClone;

    RemapInstruction(ElseClone, ElseVMap, RF_IgnoreMissingLocals);
    ElseClone->insertBefore(ElseTerm);
    ElseVMap[&Instr] = ElseClone;

    if (ThenClone->getType()->isVoidTy()) {
      ToRemove.push_back(&Instr);
      continue;
    }
    // 产生值的指令不需要 TAIL 中的槽*但是*它们可以从上下文中使用，
    // 所以总是生成一个 PHI，并让进一步的优化来进行清理

    PHINode *Phi = PHINode::Create(ThenClone->getType(), 2);
    Phi->addIncoming(ThenClone, ThenTerm->getParent());
    Phi->addIncoming(ElseClone, ElseTerm->getParent());
    TailVMap[&Instr] = Phi;

    ReMapper[&Instr] = Phi;

    ReplaceInstWithInst(Tail, IIT, Phi);
  }

  // Purge instructions that don't produce any value
  for (auto *I : ToRemove)
    I->eraseFromParent();

  ++DuplicateBBCount;
}

PreservedAnalyses DuplicateBB::run(llvm::Function &F,
                                   llvm::FunctionAnalysisManager &FAM) {
  BBToSingleRIVMap Targets = findBBsToDuplicate(F, FAM.getResult<RIV>(F));

  // This map is used to keep track of the new bindings. Otherwise, the
  // information from RIV will become obsolete.
  ValueToPhiMap ReMapper;

  // Duplicate
  for (auto &BB_Ctx : Targets) {
    cloneBB(*std::get<0>(BB_Ctx), std::get<1>(BB_Ctx), ReMapper);
  }

  DuplicateBBCountStats = DuplicateBBCount;
  return (Targets.empty() ? llvm::PreservedAnalyses::all()
                          : llvm::PreservedAnalyses::none());
}

//------------------------------------------------------------------------------
// New PM Registration
//------------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getDuplicateBBPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "duplicate-bb", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "duplicate-bb") {
                    FPM.addPass(DuplicateBB());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getDuplicateBBPluginInfo();
}
