#include "cscd70/framework.h"


//定义一个表示变量的类 Variable
namespace {

class Variable {
private:
  const Value *_val; // LLVM值的指针，表示变量

public:
  Variable(const Value *val) : _val(val) {} // 构造函数

  // 比较操作符，用于判断两个变量是否相同
  bool operator==(const Variable &val) const { return _val == val.getValue(); }

  // 获取变量值的函数
  const Value *getValue() const { return _val; }

  // 友元函数，用于输出变量
  friend raw_ostream &operator<<(raw_ostream &outs, const Variable &val);
};

// 重载输出操作符，用于打印变量
raw_ostream &operator<<(raw_ostream &outs, const Variable &var) {
  outs << "[]";
  var._val->printAsOperand(outs, false);
  outs << "]";
  return outs;
}

} // namespace


//为 Variable 类定义哈希函数
namespace std {
template <> struct hash<Variable> {
  std::size_t operator()(const Variable &var) const {
    std::hash<const Value *> value_ptr_hasher; // 值指针的哈希器

    std::size_t value_hash = value_ptr_hasher((var.getValue()));
    return value_hash; // 返回变量值的哈希
  }
};
}; // namespace std


namespace {
class Liveness final
    : public dfa::Framework<Variable, dfa::Direction::Backward> {

protected:
// 定义初始条件函数
  virtual BitVector IC() const override {
    // OUT[B] = \Phi (空集)
    // 初始条件OUT[B]为假集合
    return BitVector(_domain.size(), false);
  }

  // 定义边界条件函数
  virtual BitVector BC() const override {
    // OUT[ENTRY] = \Phi(空集)
    // 边界条件OUT[ENTRY]为假集合
    return BitVector(_domain.size(), false);
  }
// 定义meet操作函数  
  virtual BitVector MeetOp(const BasicBlock &bb) const override {

    // 此处应该是集合并运算后的结果
    BitVector result(_domain.size(), false);

     // 遍历所有后继基本块
    for (const BasicBlock *block : successors(&bb)) {
        // 获取后继基本块的第一条指令的IN集
      // 通常来讲，所有后驱基础块的第一条指令的IN集合就是当前基础块的OUT集
      const Instruction &first_inst_in_block = block->front();
      BitVector curr_bv = _inst_bv_map.at(&first_inst_in_block);

       // 处理PHI指令
      // 但这里要对含phi指令的基础块作特殊处理
      for (auto phi_iter = block->phis().begin();
           phi_iter != block->phis().end(); phi_iter++) {

        const PHINode &phi_inst = *phi_iter;

        // 遍历PHI指令的前驱基本块
        for (auto phi_inst_iter = phi_inst.block_begin();
             phi_inst_iter != phi_inst.block_end(); phi_inst_iter++) {
          // 获取PHI指令中的各个前驱基础块
          BasicBlock *const &curr_bb = *phi_inst_iter;

          // 处理不是当前基本块的前驱基本块
          // 如果当前前驱基础块不是现在的基础块
          if (curr_bb != &bb) {
            const Value *curr_val = phi_inst.getIncomingValueForBlock(curr_bb);
            // 如果当前值在domain中存在
            int idx = getDomainIndex(Variable(curr_val));
            if (idx != -1) {
              // 将临时变量中对应变量的bit设置为false
              assert(curr_bv[idx] = true);
              curr_bv[idx] = false;
            }
          }
        }
      }
      // 与临时变量做集合并操作
      result |= curr_bv;
    }
    return result;
  }


  // 定义传递函数
  virtual bool TransferFunc(const Instruction &inst,
                            const BitVector &ibv,
                            BitVector &obv) override {

    // ibv 传入 out集合，obv传入 in集合
    BitVector new_obv = ibv;

    // use 操作
    for (auto iter = inst.op_begin(); iter != inst.op_end(); iter++) {
      const Value *val = dyn_cast<Value>(*iter);
      assert(val != NULL);
      // 如果当前Variable存在domain
      if (_domain.find(val) != _domain.end())
        new_obv[getDomainIndex(val)] = true;
    }

    // def 操作，不是所有的指令都会定值，例如ret,所以设置条件判断
    if (getDomainIndex(&inst) != -1) {
      new_obv[getDomainIndex(&inst)] = false;
    }


    // 判断是否发生变化
    bool hasChanged = new_obv != obv;

    obv = new_obv;
    return hasChanged;
  }

   // 从指令初始化域
  virtual void
  InitializeDomainFromInstruction(const Instruction &inst) override {
    for (auto iter = inst.op_begin(); iter != inst.op_end(); iter++) {
      if (isa<Instruction>(*iter) || isa<Argument>(*iter)) {
        _domain.emplace(Variable(*iter));
      }
    }
  }

public:
  static char ID;
  Liveness() : dfa::Framework<domain_element_t, direction_c>(ID) {}
  virtual ~Liveness() override {}
};

char Liveness::ID = 1;
RegisterPass<Liveness> Y("liveness", "Liveness");

} // namespace