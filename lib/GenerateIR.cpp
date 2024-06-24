
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

int main() {

    LLVMContext Context;

    //创建一个Module
    auto M = std::make_unique<Module>("example",Context);

    //创建函数类型：int(int,int)
    FunctionType *FuncType = FunctionType::get(Type::getInt32Ty(Context),
                                                {Type::getInt32Ty(Context),Type::getInt32Ty(Context)},
                                                false);

    //创建具有外部链接的函数
    Function *F = Function::Create(FuncType,Function::ExternalLinkage,"add",M.get());

    //创建基本块的入口
    BasicBlock *BB = BasicBlock::Create(Context,"entry",F);
    IRBuilder<> Builder(BB);

    //获取函数的参数
    Function::arg_iterator Args = F->arg_begin();
    Value *Arg1 = &*Args++;
    Arg1->setName("a");

    Value *Arg2 = &*Args++;
    Arg2->setName("b");

    //创建一个add 指令和 返回结果
    Value * Sum = Builder.CreateAdd(Arg1,Arg2,"sum");
    Builder.CreateRet(Sum);

    //验证function
    verifyFunction(*F);

    M->print(outs(),nullptr);

    return 0;
   

}