// 编译命令：
// /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++
// -g ../llvm/GetAddress.cpp `llvm-config --cxxflags --ldflags --system-libs
// --libs core` -fno-rtti -o GetAddress
// ./GetAddress

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include <vector>

using namespace llvm;

static LLVMContext Context;
static Module *ModuleOb = new Module("my_comiple", Context);
static std::vector<std::string> FunArgs;

Function *createFunc(IRBuilder<> &Builder, std::string Name) {
  Type *u32Ty = Type::getInt32Ty(Context);
  ElementCount EC = ElementCount::getFixed(2);
  Type *vecTy = VectorType::get(u32Ty, EC);
  Type *ptrTy = vecTy->getPointerTo(0);

  FunctionType *funcType =
      FunctionType::get(Builder.getInt32Ty(), ptrTy, false);
  Function *fooFunc =
      Function::Create(funcType, Function::ExternalLinkage, Name, ModuleOb);

  return fooFunc;
}

void setFuncArgs(Function *fooFunc, std::vector<std::string> FunArgs) {
  unsigned Idx = 0;
  Function::arg_iterator AI, AE;
  for (AI = fooFunc->arg_begin(), AE = fooFunc->arg_end(); AI != AE;
       ++AI, ++Idx) {
    AI->setName(FunArgs[Idx]);
  }
}

BasicBlock *createBB(Function *fooFunc, std::string Name) {
  return BasicBlock::Create(Context, Name, fooFunc);
}

Value *createArith(IRBuilder<> &Builder, Value *L, Value *R) {
  return Builder.CreateMul(L, R, "multmp");
}

// 创建一个获取元素指针（GEP）指令。
// 参数：
//   Builder: IR构建器，用于生成LLVM IR指令。
//   Base: 基址，即起始指针。
//   Offset: 偏移量，表示从基址开始要访问的元素的偏移。
// 返回值：生成的 GEP 指令。
Value *getGEP(IRBuilder<> &Builder, Value *Base, Value *Offset) {
  // 使用 Builder 创建 GEP 指令。指定元素类型为 32 位整型（i32），
  // 基址为 Base，偏移量为 Offset。结果指令的名称设置为 "a1"。
  return Builder.CreateGEP(Builder.getInt32Ty(), Base, Offset, "a1");
}

Value *getLoad(IRBuilder<> &Builder, Value *Address) {
  Type *Int32Ty = Type::getInt32Ty(Context);
  return Builder.CreateLoad(Int32Ty, Address, "load");
}

void getStore(IRBuilder<> &Builder, Value *Address, Value *V) {
  Builder.CreateStore(V, Address);
}

Value *getInsertElement(IRBuilder<> &Builder, Value *Vec, Value *Val,
                        Value *Index) {
  return Builder.CreateInsertElement(Vec, Val, Index);
}

Value *getExtractElement(IRBuilder<> &Builder, Value *Vec, Value *Index){
    return Builder.CreateExtractElement(Vec,Index);
}

int main(int argc, char *argv[]) {
  FunArgs.push_back("a");
  static IRBuilder<> Builder(Context);
  Function *fooFunc = createFunc(Builder, "foo");
  setFuncArgs(fooFunc, FunArgs);

  Value *Base = fooFunc->arg_begin();
  BasicBlock *entry = createBB(fooFunc, "entry");
  Builder.SetInsertPoint(entry);

  //   //getelementptr
  //   Value *gep = getGEP(Builder, Base, Builder.getInt32(1));
  //   //load
  //   Value *load = getLoad(Builder, gep);
  //   //store
  //   Value *constant = Builder.getInt32(19);
  //   Value *val = createArith(Builder, load, constant);
  //   getStore(Builder, gep, val);
  //   Builder.CreateRet(val);

// //Inserting a scalar into a vector
//   Value *Vec = fooFunc->arg_begin();
//   for (unsigned int i = 0; i < 4; i++) {
//     Value *V = getInsertElement(Builder, Vec, Builder.getInt32((i + 1) * 10),
//                                 Builder.getInt32(i));
//   }
//   Builder.CreateRet(Builder.getInt32(0));

Value *Vec = fooFunc->arg_begin();
SmallVector<Value *,4> V;
for(unsigned int i = 0; i < 4; i++){
    Value *Element =  getExtractElement(Builder,Vec,Builder.getInt32(i));
    // V.push_back(Element);
}

// Value *add1 = createArith(Builder,V[0],V[1]);
// Value *add2 = createArith(Builder,add1,V[2]);
// Value *add = createArith(Builder,add2,V[3]);
// Builder.CreateRet(add);

  verifyFunction(*fooFunc);
  // 打印整个模块的 IR 到标准输出
  ModuleOb->print(outs(), nullptr);
  return 0;
}
