#pragma once

#include <cassert>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include <llvm/ADT/BitVector.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace dfa {

// 分析方向枚举，用于确定数据流分析是向前还是向后
enum class Direction { Forward, Backward };

// 数据流分析框架的定义
//  @tparam TDomainElement 数据流分析的域元素类型
//  @tparam TDirection 分析的方向（向前或向后）
template <typename TDomainElement, Direction TDirection>
class Framework : public FunctionPass {

// 定义一个宏，用于根据分析方向启用特定的方法
//  @param dir 分析的方向
//  @param ret 返回类型
#define METHOD_ENABLE_IF_DIRECTION(dir, ret) \
  template <Direction _TDirection = TDirection> \
  typename std::enable_if<_TDirection == dir, ret>::type

protected:
  // 定义域元素类型的别名
  typedef TDomainElement domain_element_t;
  // 分析方向的静态常量
  static constexpr Direction direction_c = TDirection;

  // 域集合，存储分析中的所有元素
  std::unordered_set<TDomainElement> _domain;

  // 指令到比特向量的映射
  // 映射从指令指针到比特向量（用于存储OUT集合）
  std::unordered_map<const Instruction *, BitVector> _inst_bv_map;

  // 返回初始条件的虚函数，由子类实现
  virtual BitVector IC() const = 0;

  // 返回边界条件的虚函数，由子类实现
  virtual BitVector BC() const = 0;

private:
  // 打印带有掩码的域集合
  void printDomainWithMask(const BitVector &mask) const {
    outs() << "{";
    assert(mask.size() == _domain.size() &&
           "掩码大小必须等于域的大小");
    unsigned mask_idx = 0;
    for (const auto &elem : _domain) {
      if (!mask[mask_idx++]) {
        continue;
      }
      outs() << elem << ",";
    }
    outs() << "}";
  }

  // 打印与指令相关的比特向量
  void printInstBV(const Instruction &inst) const {
    const BasicBlock *const pbb = inst.getParent();
    if (&inst == &(*InstTraversalOrder(*pbb).begin())) {
      auto meet_operands = MeetOperands(*pbb);
      if (meet_operands.begin() == meet_operands.end()) {
        outs() << "BC:\t";
        printDomainWithMask(BC());
      } else {
        outs() << "MeetOp:\t";
        printDomainWithMask(MeetOp(*pbb));
      }
      outs() << "\n";
    }
    outs() << "Instruction: " << inst << "\n";
    outs() << "\t";
    printDomainWithMask(_inst_bv_map.at(&inst));
    outs() << "\n";
  }

protected:
  // 打印函数中指令和比特向量的映射
  void printInstBVMap(const Function &F) const {
    outs() << "***********************************\n";
    outs() << "* Instruction-BitVector Mapping    \n";
    outs() << "***********************************\n";
    for (const auto &inst : instructions(F)) {
      printInstBV(inst);
    }
  }

  // 遇见操作符和传递函数的定义
  // 根据分析方向启用不同的方法
  METHOD_ENABLE_IF_DIRECTION(Direction::Forward, pred_const_range)
  MeetOperands(const BasicBlock &bb) const { return predecessors(&bb); }

  METHOD_ENABLE_IF_DIRECTION(Direction::Backward, succ_const_range)
  MeetOperands(const BasicBlock &bb) const { return successors(&bb); }

  // 遇见操作的虚函数，由子类实现
  virtual BitVector MeetOp(const BasicBlock &bb) const = 0;

  // 传递函数的虚函数，由子类实现
  virtual bool TransferFunc(const Instruction &inst, const BitVector &ibv,
                            BitVector &obv) = 0;

private:
  // 根据分析方向定义基本块和指令的遍历顺序
  METHOD_ENABLE_IF_DIRECTION(Direction::Forward, iterator_range<SymbolTableList<BasicBlock>::const_iterator>)
  BBTraversalOrder(const Function &F) const {
    return make_range(F.begin(), F.end());
  }

  METHOD_ENABLE_IF_DIRECTION(Direction::Forward, iterator_range<SymbolTableList<Instruction>::const_iterator>)
  InstTraversalOrder(const BasicBlock &bb) const {
    return make_range(bb.begin(), bb.end());
  }

  METHOD_ENABLE_IF_DIRECTION(Direction::Backward, iterator_range<SymbolTableList<Instruction>::const_reverse_iterator>)
  InstTraversalOrder(const BasicBlock &bb) const {
    return make_range(bb.getInstList().rbegin(), bb.getInstList().rend());
  }

protected:
  // 获取域中元素的索引
  int getDomainIndex(const TDomainElement &elem) const {
    auto inst_dom_iter = _domain.find(elem);
    if (inst_dom_iter == _domain.end())
      return -1;
    int idx = 0;
    for (auto dom_iter = _domain.begin(); inst_dom_iter != dom_iter; ++dom_iter)
      ++idx;
    return idx;
  }

  // 遍历控制流图
  bool traverseCFG(const Function &func) {
    bool transform = false;
    for (const BasicBlock &basicBlock : BBTraversalOrder(func)) {
      BitVector ibv;
      if (&basicBlock == &(*BBTraversalOrder(func).begin()))
        ibv = BC();
      else
        ibv = MeetOp(basicBlock);
      for (const Instruction &inst : InstTraversalOrder(basicBlock)) {
        transform |= TransferFunc(inst, ibv, _inst_bv_map[&inst]);
        ibv = _inst_bv_map[&inst];
      }
    }
    return transform;
  }

public:
  // 构造函数
  Framework(char ID) : FunctionPass(ID) {}
  // 析构函数
  virtual ~Framework() override {}

  // 分析使用的设置
  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  }

protected:
  // 从指令初始化域的方法，由子类实现
  virtual void InitializeDomainFromInstruction(const Instruction &inst) = 0;

public:
  // 在函数上运行数据流分析
  virtual bool runOnFunction(Function &F) override final {
    for (const auto &inst : instructions(F)) {
      InitializeDomainFromInstruction(inst);
    }
    for (const auto &inst : instructions(F)) {
      _inst_bv_map.emplace(&inst, IC());
    }
    while (traverseCFG(F)) {
    }
    printInstBVMap(F);
    return false;
  }

#undef METHOD_ENABLE_IF_DIRECTION
};

} // namespace dfa
