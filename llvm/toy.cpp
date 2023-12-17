#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

int main(int argc, char *argv[]) {

  using namespace llvm;
  LLVMContext Context;

  std::unique_ptr<Module> module =
      std::make_unique<Module>("my_module", Context);

  IRBuilder<> builder(Context);

  // 创建一个函数类型（这里示例为无参数和返回值）
  FunctionType *funcType = FunctionType::get(builder.getVoidTy(), false);

  // 在模块中创建函数
  Function *func = Function::Create(funcType, Function::ExternalLinkage,
                                    "my_function", module.get());

 //创建基本块
  BasicBlock *bb = BasicBlock::Create(Context, "entry", func);

  builder.SetInsertPoint(bb);

  //完成基本块，添加返回指令
  builder.CreateRetVoid();

  //打印模块的IR 到标准输出
  module->print(outs(),nullptr);
  return 0;

}
