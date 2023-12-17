//https://github.com/Kiprey/Skr_Learning/blob/master/week7-8/Assignment1-Introduction_to_LLVM/LocalOpts/LocalOpts.cpp
#include "llvm/IR/Constant.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

namespace {

class LocalOpts final : public ModulePass {
public:
  static char ID;

  LocalOpts() : ModulePass(ID) {}
  virtual ~LocalOpts() override {}

  virtual void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }

  virtual bool runOnModule(Module &M) override {
    bool transform = false;
    for (Function &func : M) {
      if (runOnFunction(func))
        transform = true;
    }
  }

private:
  int algebraic_identity_num = 0;
  int strength_reduction_num = 0;
  int multi_inst_optimization_num = 0;

  bool runOnBasicBlock(BasicBlock &B) {
    std::list<Instruction *> deleteInstr;
    for (BasicBlock::iterator iter = B.begin(); iter != B.end(); iter++) {
      Instruction &inst = *iter;
      if (inst.isBinaryOp()) {
        switch (inst.getOpcode()) {
        case Instruction::Add:

        case Instruction::Sub:
          // x+0=0+x = x
          for (int i = 0; i < 2; ++i) {
            if (ConstantInt *val = dyn_cast<ConstantInt>(inst.getOperand(i))) {
              if (val->getZExtValue() == 0) {
                ++algebraic_identity_num;
                Value *another_val = inst.getOperand(i == 0 ? 1 : 0);
                inst.replaceAllUsesWith(another_val);
                deleteInstr.push_back(&inst);
                break;
              }
            }
          }
          // b = a+1;c=b-1 ==> b=a+1;c=a
          if (ConstantInt *val = dyn_cast<ConstantInt>(inst.getOperand(1))) {
            Instruction *another_inst =
                dyn_cast<Instruction>(inst.getOperand(0));
            if (another_inst) {
              if (inst.getOpcode() + another_inst->getOpcode() ==
                  Instruction::Add + Instruction::Sub && another_inst->getOperand(1) == val){
                    ++multi_inst_optimization_num;
                    inst.replaceAllUsesWith(another_inst->getOperand(0));
                    deleteInstr.push_back(&inst);
                    break;
                  }
            }
          }
        case Instruction::Mul:
               //x * 2 ===> x<<1;
               for(int i = 0; i < 2; i++){
                   if(ConstantInt *val = dyn_cast<ConstantInt>(inst.getOperand(i))){
                       if(val->getZExtValue() == 2){
                          ++strength_reduction_num;
                         Value *another_val = inst.getOperand(i == 0 ? 1 : 0);
                         
                         IRBuilder<> builder(&inst);
                         Value* val = builder.CreateShl(another_val,1);
                         inst.replaceAllUsesWith(val);
                         deleteInstr.push_back(&inst);
                         break;
                       }
                   }
               }  
        default:
          break;
        }
      }
    }


     for(Instruction *inst : deleteInstr){
        if(inst->isSafeToRemove())
             inst->eraseFromParent();
     }

     return true;
  }


  virtual bool runOnFunction(Function &F){
    bool transform = false;
    for(BasicBlock &block : F){
        if(runOnBasicBlock(block)){
            transform = true;
        }
    }
    return transform;
  }


  void dumpInformation(){
       outs() << "Transformations applied:\n";
		outs() << "\tAlgebraic Identity: " << algebraic_identity_num << "\n";
		outs() << "\tStrength Reduction: " << strength_reduction_num << "\n";
		outs() << "\tMulti-Inst Optimization: " << multi_inst_optimization_num << "\n";
  }


 
};

char LocalOpts::ID = 0;
RegisterPass <LocalOpts> X ("Local-Opts",
	"CSCD70: Local Opts");


} // namespace
