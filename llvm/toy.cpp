// 包含以下功能实现：
// Creating an LLVM module
// Emitting a function in a module
// Adding a block to a function
// Emitting a global variable
// Emitting a return statement
// Emitting function arguments
// Emitting a simple arithmetic statement in a basic block Emitting if-else condition IR
// Emitting LLVM IR for loops
// 编译命令：
// /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++ -g ../llvm/toy.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o toy
// ./toy

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

// 创建一个 if-else 控制流结构，并返回结果值。
// 参数：
//   Builder: LLVM IR 构建器，用于创建指令。
//   List: 包含三个基本块的列表，分别代表 then 块、else 块和合并块。
//   VL: 包含两个值的列表，分别是条件判断和 if-else 语句中使用的参数。
Value *createIfElse(IRBuilder<> &Builder, BBList List, ValList VL) {
  // 提取条件和参数。
  Value *Condtn = VL[0];
  Value *Arg1 = VL[1];

  // 提取基本块。
  BasicBlock *ThenBB = List[0];
  BasicBlock *ElseBB = List[1];
  BasicBlock *MergeBB = List[2];

  // 创建条件跳转指令。
  Builder.CreateCondBr(Condtn, ThenBB, ElseBB);

  // 处理 then 块。
  Builder.SetInsertPoint(ThenBB);
  // 在 then 块中创建加法指令，并跳转到合并块。
  Value *ThenVal = Builder.CreateAdd(Arg1, Builder.getInt32(1), "thenaddtmp");
  Builder.CreateBr(MergeBB);

  // 处理 else 块。
  Builder.SetInsertPoint(ElseBB);
  // 在 else 块中创建加法指令，并跳转到合并块。
  Value *ElseVal = Builder.CreateAdd(Arg1, Builder.getInt32(2), "elseaddtmp");
  Builder.CreateBr(MergeBB);

  // 在合并块中创建 PHI 节点。
  unsigned PhiBBSize = List.size() - 1;
  Builder.SetInsertPoint(MergeBB);
  PHINode *Phi = Builder.CreatePHI(Type::getInt32Ty(Context), PhiBBSize, "iftmp");

  // 为 PHI 节点添加来自 then 和 else 块的值。
  Phi->addIncoming(ThenVal, ThenBB);
  Phi->addIncoming(ElseVal, ElseBB);

  // 返回 PHI 节点，它代表 if-else 结构的结果。
  return Phi;
}

// 创建一个循环结构，并返回循环体中的一个运算结果。
// 参数：
//   Builder: IR构建器，用于生成LLVM IR指令。
//   List: 包含两个基本块的列表，第一个是循环体，第二个是循环结束后的代码块。
//   VL: 包含循环体中用到的值的列表。
//   StartVal: 循环的起始值。
//   EndVal: 循环的结束条件值。
Value *createLoop(IRBuilder<> &Builder, BBList List, ValList VL, Value *StartVal, Value *EndVal) {
  // 获取当前插入点的基本块作为循环的前置块。
  BasicBlock *PreheaderBB = Builder.GetInsertBlock();

  // 从值列表中获取循环体中将使用的值。
  Value *val = VL[0];

  // 获取循环体基本块。
  BasicBlock *LoopBB = List[0];

  // 创建从前置块到循环体的跳转。
  Builder.CreateBr(LoopBB);

  // 将插入点设置为循环体。
  Builder.SetInsertPoint(LoopBB);

  // 创建 PHI 节点，用于循环迭代变量。
  PHINode *IndVar = Builder.CreatePHI(Type::getInt32Ty(Context), 2, "i");

  // 设置迭代变量的起始值。
  IndVar->addIncoming(StartVal, PreheaderBB);

  // 在循环体中创建加法运算（例如，每次循环加 5）。
  Value *Add = Builder.CreateAdd(val, Builder.getInt32(5), "addtmp");

  // 设置循环步长。
  Value *StepVal = Builder.getInt32(1);

  // 计算下一个迭代值。
  Value *NextVal = Builder.CreateAdd(IndVar, StepVal, "nextval");

  // 创建循环结束条件的比较运算。
  Value *EndCond = Builder.CreateICmpULT(IndVar, EndVal, "endcond");

  // 将比较结果转换为 int32 类型，以便用作条件。
  EndCond = Builder.CreateIntCast(EndCond, Type::getInt32Ty(Context), true, "casttmp");

  // 确保循环条件是非零的。
  EndCond = Builder.CreateICmpNE(EndCond, Builder.getInt32(0), "loopcond");

  // 获取循环体结束时的基本块。
  BasicBlock *LoopEndBB = Builder.GetInsertBlock();

  // 获取循环结束后的代码块。
  BasicBlock *AfterBB = List[1];

  // 创建条件跳转，决定是继续循环还是退出循环。
  Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

  // 将插入点设置为循环后的代码块。
  Builder.SetInsertPoint(AfterBB);

  // 更新 PHI 节点的下一个迭代值。
  IndVar->addIncoming(NextVal, LoopEndBB);

  // 返回循环体中的加法运算结果。
  return Add;
}


int main(int argc, char *argv[]) {

  // 添加函数参数的名称
FunArgs.push_back("a");
FunArgs.push_back("b");

// 创建一个 IR 构建器对象，用于生成 LLVM IR 指令
static IRBuilder<> Builder(Context);

// 创建一个全局变量 'x'
GlobalVariable *gVar = createGlob(Builder, "x");

// 创建一个名为 'foo' 的函数，返回类型为 int，参数类型为 int
Function *fooFunc = createFunc(Builder, "foo");

// 创建函数的入口基本块 'entry'
BasicBlock *entry = createBB(fooFunc, "entry");

// 将指令插入点设置为入口基本块
Builder.SetInsertPoint(entry);


//==-------------------------- Add Loop START------------------------------------------==//

Function::arg_iterator AI = fooFunc->arg_begin();
Value *Arg1 = AI++;
Value *Arg2 = AI;

Value *Constant = Builder.getInt32(16);
Value *val = createArith(Builder,Arg1,Constant);
ValList VL;
VL.push_back(Arg1);

BBList List;
BasicBlock *LoopBB = createBB(fooFunc,"loop");
BasicBlock *AfterBB = createBB(fooFunc,"afterloop");
List.push_back(LoopBB);
List.push_back(AfterBB);

Value *StartVal = Builder.getInt32(1);
Value *Res = createLoop(Builder,List,VL,StartVal,Arg2);

// 创建一个返回指令，返回 if-else 结构的结果
Builder.CreateRet(Res);




//==---------------------------Add Loop END -----------------------------------------==//



//==---------------------------Add If-Then-Else START -------------------------------==//


// // 获取函数的第一个参数
// Value *Arg1 = fooFunc->arg_begin();

// // 创建一个 int32 类型的常量 16
// Value *constant = Builder.getInt32(16);

// // 创建一个乘法运算，将 Arg1 与常量 16 相乘
// Value *val = createArith(Builder, Arg1, constant);

// // 创建另一个 int32 类型的常量 100
// Value *val2 = Builder.getInt32(100);

// // errs() << "type :" << (val->getType() == val2->getType());
// // 创建一个比较指令，比较 val 是否小于 val2
// Value *Compare = Builder.CreateICmpULT(val, val2, "cmptmp");

// Compare = Builder.CreateIntCast(Compare, Type::getInt32Ty(Context), true /* isSigned */, "casttmp");

// // 创建一个比较指令，检查 Compare 是否不等于 0
// Value *Condtn = Builder.CreateICmpNE(Compare, Builder.getInt32(0), "ifcond");

// // 创建一个值列表，用于存储 if-else 结构中使用的条件和参数
// ValList VL;
// VL.push_back(Condtn);
// VL.push_back(Arg1);

// // 创建 if-else 结构的三个基本块：then 块、else 块和合并块
// BasicBlock *ThenBB = createBB(fooFunc, "then");
// BasicBlock *ElseBB = createBB(fooFunc, "else");
// BasicBlock *MergeBB = createBB(fooFunc, "ifcont");

// // 将这些基本块添加到列表中
// BBList List;
// List.push_back(ThenBB);
// List.push_back(ElseBB);
// List.push_back(MergeBB);

// // 创建 if-else 结构并获取其结果
// Value *v = createIfElse(Builder, List, VL);
// 创建一个返回指令，返回 if-else 结构的结果
// Builder.CreateRet(v);
//==---------------------------Add If-Then-Else END -------------------------------==//




// 验证函数的正确性
verifyFunction(*fooFunc);

// 打印整个模块的 IR 到标准输出
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
