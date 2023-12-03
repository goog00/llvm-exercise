#include "DynamicCallCounter.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

#define DEBUG_TYPE "dynamic-cc"

Constant *CreateGlobalCounter(Module &M, StringRef GlobalValName) {
  auto &CTX = M.getContext();

  // 在module中插入一个声明
  Constant *NewGlobalVar =
      M.getOrInsertGlobal(GlobalValName, IntegerType::getInt32Ty(CTX));

  // 初始化上一步的声明
  GlobalVariable *NewGV = M.getNamedGlobal(GlobalValName);

  NewGV->setLinkage(GlobalValue::CommonLinkage);
  NewGV->setAlignment(MaybeAlign(4));
  NewGV->setInitializer(llvm::ConstantInt::get(CTX, APInt(32, 0)));

  return NewGlobalVar;
}

// DynamicCallCounter implementation

bool DynamicCallCounter::runOnModule(Module &M) {
  bool Instrumented = false;

  // key是函数名
  llvm::StringMap<Constant *> CallCounterMap;

  llvm::StringMap<Constant *> FuncNameMap;

  auto &CTX = M.getContext();

  // step1:遍历module， 统计调用次数的代码
  for (auto &F : M) {
    if (F.isDeclaration()) {
      continue;
    }

    // 构建基本块入口的第一条指令
    IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());

    // 创建一个全局变量CounterName 去计数这个函数的调用
    std::string CounterName = "CounterFor_" + std::string(F.getName());

    Constant *Var = CreateGlobalCounter(M, CounterName);

    CallCounterMap[F.getName()] = Var;

    // 创建一个全局变量FuncName ,用来保存这个函数的名字
    auto FuncName = Builder.CreateGlobalStringPtr(F.getName());
    FuncNameMap[F.getName()] = FuncName;

    // 每次执行此函数时，注入指令以增加调用计数
    LoadInst *Load2 = Builder.CreateLoad(IntegerType::getInt32Ty(CTX), Var);
    Value *Inc2 = Builder.CreateAdd(Builder.getInt32(1), Load2);
    Builder.CreateStore(Inc2, Var);

    LLVM_DEBUG(dbgs() << " Instrumented: " << F.getName() << "\n");

    Instrumented = true;
  }

  if (false == Instrumented)
    return Instrumented;

  // step2:注入printf的声明
  //-------------------
  // 在IR module 中创建一个声明：
  //         declare i32 @printf(i8*,...)
  // 对应 C 语言的代码 如：
  //         int printf(char *,...);

  // 设置插入函数的参数
  PointerType *PrintArgTy = PointerType::getUnqual(Type::getInt8Ty(CTX));

  // 构建函数：第一个参数为返回值，第二个是函数的参数
  FunctionType *PrintfTy =
      FunctionType::get(IntegerType::getInt32Ty(CTX), PrintArgTy, true);

  FunctionCallee Printf = M.getOrInsertFunction("printf", PrintfTy);

  // 设置函数的属性
  Function *PrintfF = dyn_cast<Function>(Printf.getCallee());
  PrintfF->setDoesNotThrow();
  PrintfF->addParamAttr(0, Attribute::NoCapture);
  PrintfF->addParamAttr(0, Attribute::ReadOnly);

  // step 3: 设置step2构建的函数的body
  llvm::Constant *ResultFormatStr =
      llvm::ConstantDataArray::getString(CTX, "%-20s %-10lu\n");

  Constant *ResultFormatStrVar =
      M.getOrInsertGlobal("ResultFormatStrIR", ResultFormatStr->getType());

  //setInitializer(ResultFormatStrVar) 会异常
  dyn_cast<GlobalVariable>(ResultFormatStr)->setInitializer(ResultFormatStr);

  std::string out = "";
  out += "=================================================\n";
  out += "LLVM-EXERCISE: dynamic analysis results\n";
  out += "=================================================\n";
  out += "NAME                 #N DIRECT CALLS\n";
  out += "-------------------------------------------------\n";

  llvm::Constant *ResultHeaderStr =
      llvm::ConstantDataArray::getString(CTX, out.c_str());

  Constant *ResultHeaderStrVar =
      M.getOrInsertGlobal("ResultHeaderStrIR", ResultHeaderStr->getType());

   dyn_cast<GlobalVariable>(ResultHeaderStrVar)->setInitializer(ResultHeaderStr);


  // step4 : 定义printf的包装函数 ，用来打印结果
  //-------------------------------------
  // 定义“printf_wrapper" 打印保存在FuncNameMap 和 CallCounterMap中数据。
  // 与C++ 的函数相似 如：
  //  ```
  //    void printf_wrapper() {
  //      for (auto &item : Functions)
  //        printf("llvm-tutor): Function %s was called %d times. \n",
  //        item.name, item.count);
  //    }
  // ```
  // (item.name comes from FuncNameMap, item.count comes from
  // CallCounterMap)

  FunctionType *PrintfWrapperTy =
      FunctionType::get(llvm::Type::getVoidTy(CTX), {}, false);

  Function *PrintfWrapperF = dyn_cast<Function>(
      M.getOrInsertFunction("print_wrapper", PrintfWrapperTy).getCallee());

  llvm::BasicBlock *RetBlock =
      llvm::BasicBlock::Create(CTX, "enter", PrintfWrapperF);

  IRBuilder<> Builder(RetBlock);

  // 开始插入printf的调用函数
  // printf 需要i8* ,需要把输入的字符串转换到相应的类型
  llvm::Value *ResultHeaderStrPtr =
      Builder.CreatePointerCast(ResultHeaderStrVar, PrintArgTy);
  llvm::Value *ResultFormatStrPtr =
      Builder.CreatePointerCast(ResultFormatStrVar, PrintArgTy);

  Builder.CreateCall(Printf, {ResultHeaderStrPtr});


  
  LoadInst *LoadCounter;
  for (auto &item : CallCounterMap) {
    LoadCounter = Builder.CreateLoad(IntegerType::getInt32Ty(CTX), item.second);
    Builder.CreateCall(
        Printf, {ResultFormatStrPtr, FuncNameMap[item.first()], LoadCounter});
  }

  // 最后，插入return 指令
  Builder.CreateRetVoid();

  // step5: 在每个module的最后调用‘printf_wrapper’
  appendToGlobalDtors(M, PrintfWrapperF, 0);

  return true;
}

PreservedAnalyses DynamicCallCounter::run(llvm::Module &M,
                                          llvm::ModuleAnalysisManager &) {
  bool Changed = runOnModule(M);

  return (Changed ? llvm::PreservedAnalyses::none()
                  : llvm::PreservedAnalyses::all());
}

llvm::PassPluginLibraryInfo getDynamicCallCounterPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "dynamic-cc", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "dynamic-cc") {
                    MPM.addPass(DynamicCallCounter());
                    return true;
                  }
                  return false;
                });
          }

  }; // return
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getDynamicCallCounterPluginInfo();
}