#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include <vector>

using namespace llvm;

static LLVMContext Context;
static Module *ModuleOb = new Module("my compiler", Context);

static std::vector<std::string> FunArgs;
typedef SmallVector<BasicBlock *,16> BBList;
typedef SmallVector<Value *,16> ValList;


Function *createFunc(IRBuilder<> &Builder, std::string Name) {
  std::vector<Type *> Integers(FunArgs.size(), Type::getInt32Ty(Context));

  FunctionType *funcType =
      llvm::FunctionType::get(Builder.getInt32Ty(), Integers, false);
  Function *fooFunc = llvm::Function::Create(
      funcType, llvm::Function::ExternalLinkage, Name, ModuleOb);
  return fooFunc;
}
/**
 * 在您的函数 `setFuncArgs` 中，`unsigned Idx = 0;` 用于在循环中作为索引来访问 `FunArgs` 向量。使用 `unsigned` 类型而不是 `int` 有几个原因：

1. **非负性**：`unsigned` 类型保证了变量 `Idx` 始终为非负数，这对于数组或向量的索引来说是合适的，因为索引值永远不应该是负数。

2. **一致性和安全性**：在 C++ 中，标准库容器（如 `std::vector`）的 `size()` 方法返回一个 `size_t` 类型的值，它通常是 `unsigned` 类型。使用 `unsigned` 类型的索引可以确保类型的一致性，避免类型转换时可能出现的问题。

3. **范围**：对于索引和大小，`unsigned` 类型提供的范围通常更合适。例如，32位系统上的 `unsigned int` 可以表示的最大值是 2^32-1，而 `int` 只能表示到 2^31-1。

确实，您也可以使用 `int` 类型来声明 `Idx`，尤其是在您确信 `FunArgs` 的大小不会超过 `int` 能表示的正数范围，并且您不会与需要 `unsigned` 类型的其他 API（如 `std::vector::size()`）直接交互时。然而，使用 `unsigned` 通常被视为更好的实践，因为它提供了额外的类型安全性，并且在概念上与索引的用途更匹配。
*/
void setFuncArgs(Function *fooFunc, std::vector<std::string> FunArgs) {
  int Idx = 0;
  Function::arg_iterator AI, AE;
  for (AI = fooFunc->arg_begin(), AE = fooFunc->arg_end(); AI != AE;
       ++AI, ++Idx) {
    AI->setName(FunArgs[Idx]);
  }
}

BasicBlock *createBB(Function *fooFunc, std::string Name) {
  return BasicBlock::Create(Context, Name, fooFunc);
}

GlobalVariable *createGlob(IRBuilder<> &Builder, std::string Name) {
  ModuleOb->getOrInsertGlobal(Name, Builder.getInt32Ty());
  GlobalVariable *gVar = ModuleOb->getNamedGlobal(Name);
  gVar->setLinkage(GlobalValue::CommonLinkage);
  // 设置对齐，4字节对齐
  gVar->setAlignment(MaybeAlign(4));
  return gVar;
}


Value *createArith(IRBuilder<> &Builder,Value *L, Value *R){
  return Builder.CreateMul(L,R,"multmp");
}

Value *createIfElse(IRBuilder<> &Builder, BBList List,ValList VL) {
  Value *Condtn = VL[0];
  Value *Arg1 = VL[1];
  BasicBlock *ThenBB = List[0];
  BasicBlock *ElseBB = List[1];
  BasicBlock *MergeBB = List[2];
  Builder.CreateCondBr(Condtn,ThenBB,ElseBB);

  Builder.SetInsertPoint(ThenBB);
  Value *ThenVal = Builder.CreateAdd(Arg1,Builder.getInt32(1),"thenaddtmp");
  Builder.CreateBr(MergeBB);

  Builder.SetInsertPoint(ElseBB);
  Value *ElseVal = Builder.CreateAdd(Arg1,Builder.getInt32(2),"elseaddtmp");
  Builder.CreateBr(MergeBB);

  unsigned PhiBBSize = List.size() -1 ;
  Builder.SetInsertPoint(MergeBB);
  PHINode *Phi = Builder.CreatePHI(Type::getInt32Ty(Context),PhiBBSize,"iftmp");

  Phi->addIncoming(ThenVal,ThenBB);
  Phi->addIncoming(ElseVal,ElseBB);
  return Phi;


}

int main(int argc, char *argv[]) {

  FunArgs.push_back("a");
  FunArgs.push_back("b");

  static IRBuilder<> Builder(Context);

  GlobalVariable *gVar = createGlob(Builder, "x");

  Function *fooFunc = createFunc(Builder, "foo");

  BasicBlock *entry = createBB(fooFunc, "entry");

  Builder.SetInsertPoint(entry);

  Value *Arg1 = fooFunc->arg_begin();
  Value *constant = Builder.getInt32(16);
  Value *val = createArith(Builder,Arg1,constant);

errs() << "dddddddd: " << val->getType() <<"   \n";

// assert(val->getType() == Type::getInt32Ty(Context) && "val is not int32");

  // val = Builder.CreateIntCast(val, Type::getInt32Ty(Context), false /* isSigned */, "casttmp");


  Value *val2 = Builder.getInt32(100);
  errs() << "val2: " << val2->getType() <<"   \n";

  Value *Compare = Builder.CreateICmpULT(val,val2,"cmptmp");
  Value *Condtn = Builder.CreateICmpNE(Compare,Builder.getInt32(0),"ifcond");
  
  ValList VL;
  VL.push_back(Condtn);
  VL.push_back(Arg1);

  BasicBlock *ThenBB = createBB(fooFunc,"then");
  BasicBlock *ElseBB = createBB(fooFunc,"else");
  BasicBlock *MergeBB = createBB(fooFunc,"ifcont");
  BBList List;
  List.push_back(ThenBB);
  List.push_back(ElseBB);
  List.push_back(MergeBB);
  
  Value *v = createIfElse(Builder,List,VL);

  // Builder.CreateRet(Builder.getInt32(0));
  Builder.CreateRet(v);
  verifyFunction(*fooFunc);
  //  ModuleOb->dump();
  // 打印模块的IR 到标准输出
  ModuleOb->print(outs(), nullptr);
  return 0;

  //   using namespace llvm;
  //   LLVMContext Context;

  //   std::unique_ptr<Module> module =
  //       std::make_unique<Module>("my_module", Context);

  //   IRBuilder<> builder(Context);

  //   // 创建一个函数类型（这里示例为无参数和返回值）
  //   FunctionType *funcType = FunctionType::get(builder.getVoidTy(), false);

  //   // 在模块中创建函数
  //   Function *func = Function::Create(funcType, Function::ExternalLinkage,
  //                                     "my_function", module.get());

  //  //创建基本块
  //   BasicBlock *bb = BasicBlock::Create(Context, "entry", func);

  //   builder.SetInsertPoint(bb);

  //   //完成基本块，添加返回指令
  //   builder.CreateRetVoid();

  //   //打印模块的IR 到标准输出
  //   module->print(outs(),nullptr);
  //   return 0;
}
