//https://www.bitsand.cloud/posts/llvm-pass/

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// 定义一个常量字符串，命名为"_MemoryTraceFP"，用作全局变量的名称. 文件的指针
const std::string FilePointerVarName = "_MemoryTraceFP";

// 在LLVM模块中添加一个名为"_MemoryTraceFP"的全局变量，该变量的类型是指向8位整数的指针，并且这个全局变量具有外部链接类型，可以在其他模块中访问。
void addGlobalMemoryTraceFP(llvm::Module &M) {
  auto &CTX = M.getContext();

  // 在模块M中获取或插入一个全局变量。如果该全局变量已经存在，则返回它；否则创建一个新的全局变量。
  // 全局变量的名称为FilePointerVarName，类型为指向8位整数的无限定指针。
  M.getOrInsertGlobal(FilePointerVarName,
                      PointerType::getUnqual(Type::getInt8Ty(CTX)));

  // 从模块M中获取名为FilePointerVarName的全局变量，并将其指针赋值给nameGlobal。
  GlobalVariable *nameGlobal = M.getNamedGlobal(FilePointerVarName);
  // 设置nameGlobal的链接类型为ExternalLinkage，这意味着这个全局变量在其他模块中也是可见和可访问的。
  nameGlobal->setLinkage(GlobalValue::ExternalLinkage);
}

// 添加内存追踪文件指针初始化函数
void addMemoryTraceFPInitialization(llvm::Module &M, llvm::Function &MainFunc) {
  // 获取LLVM模块的上下文
  auto &CTX = M.getContext();

  // 定义fopen函数的参数，两个参数都是int8指针类型
  std::vector<llvm::Type *> FopenArgs{
      PointerType::getUnqual(Type::getInt8Ty(CTX)),
      PointerType::getUnqual(Type::getInt8Ty(CTX))};
  // 定义fopen函数的函数类型，返回类型是int8指针类型，参数是上一步定义的参数类型
  FunctionType *FopenTy = FunctionType::get(
      PointerType::getUnqual(Type::getInt8Ty(CTX)), FopenArgs, false);
  // 在模块中插入或获取fopen函数
  FunctionCallee Fopen = M.getOrInsertFunction("fopen", FopenTy);

  // 创建一个常量字符串"memory-traces.log"
  Constant *FopenFileNameStr =
      llvm::ConstantDataArray::getString(CTX, "memory-traces.log");
  // 在模块中插入或获取一个全局变量，用于存储文件名字符串常量
  Constant *FopenFileNameStrVar =
      M.getOrInsertGlobal("FopenFileNameStr", FopenFileNameStr->getType());
  // 将文件名字符串常量设置为全局变量的初始化值
  dyn_cast<GlobalVariable>(FopenFileNameStrVar)
      ->setInitializer(FopenFileNameStr);

  // 创建一个常量字符串"w+"
  Constant *FopenModeStr = llvm::ConstantDataArray::getString(CTX, "w+");
  // 在模块中插入或获取一个全局变量，用于存储打开模式字符串常量
  Constant *FopenModeStrVar =
      M.getOrInsertGlobal("FopenModeStr", FopenModeStr->getType());
  dyn_cast<GlobalVariable>(FopenModeStrVar)->setInitializer(FopenModeStr);

  // 创建一个IRBuilder对象，用于在MainFunc函数的入口块中插入指令
  IRBuilder<> Builder(&*MainFunc.getEntryBlock().getFirstInsertionPt());
  // 创建一个指针类型转换指令，将文件名全局变量的指针类型转换为fopen函数的参数类型
  llvm::Value *FopenFileNameStrPtr = Builder.CreatePointerCast(
      FopenFileNameStrVar, FopenArgs[0], "fileNameStr");
  // 创建一个指针类型转换指令，将打开模式全局变量的指针类型转换为fopen函数的参数类型
  llvm::Value *FopenModeStrPtr =
      Builder.CreatePointerCast(FopenModeStrVar, FopenArgs[0], "modelStr");
  // 创建一个函数调用指令，调用fopen函数，传入文件名和打开模式参数，返回文件指针
  llvm::Value *FopenReturn =
      Builder.CreateCall(Fopen, {FopenFileNameStrPtr, FopenModeStrPtr});
  // 从模块中获取名为FilePointerVarName的全局变量，这个全局变量应该是一个文件指针类型，用于存储fopen函数的返回值
  GlobalVariable *FPGobal = M.getNamedGlobal(FilePointerVarName);
}

void addMemoryTraceToBB(IRBuilder<> &Builder, llvm::Function &Function,
                        llvm::Module &M, bool IsLoad) {
  auto &CTX = M.getContext();

  std::vector<llvm::Type *> FprintfArgs{
      PointerType::getUnqual(Type::getInt8Ty(CTX)),
      PointerType::getUnqual(Type::getInt8Ty(CTX))};

  FunctionType *FprintfTy =
      FunctionType::get(Type::getInt32Ty(CTX), FprintfArgs, true);

  FunctionCallee Fprintf = M.getOrInsertFunction("fprintf", FprintfTy);

  Constant *TraceLoadStr = llvm::ConstantDataArray::getString(
      CTX, "[Read] Read value 0x%lx from address %p\n");
  Constant *TraceLoadStrVar =
      M.getOrInsertGlobal("TraceLoadStr", TraceLoadStr->getType());
  dyn_cast<GlobalVariable>(TraceLoadStrVar)->setInitializer(TraceLoadStr);

  Constant *TraceStoreStr = llvm::ConstantDataArray::getString(
      CTX, "[Write] Wrote value 0x%lx to address %p\n");
  Constant *TraceStoreStrVar =
      M.getOrInsertGlobal("TraceStoreStr", TraceStoreStr->getType());
  dyn_cast<GlobalVariable>(TraceLoadStrVar)->setInitializer(TraceStoreStr);

  llvm::Value *StrPtr;
  if (IsLoad)
    StrPtr = Builder.CreatePointerCast(TraceLoadStrVar, FprintfArgs[1],
                                       "loadStrPtr");

  else
    StrPtr = Builder.CreatePointerCast(TraceStoreStrVar, FprintfArgs[1],
                                       "storeStrPtr");

  GlobalVariable *FPGlobal = M.getNamedGlobal(FilePointerVarName);
  llvm::LoadInst *FP = Builder.CreateLoad(
      PointerType::getUnqual(Type::getInt8Ty(CTX)), FPGlobal);
  Builder.CreateCall(Fprintf,
                     {FP, StrPtr, Function.getArg(1), Function.getArg(0)});
}

// 创建函数
const std::string TraceMemoryFunctionName = "_TraceMemory";
void addTraceMemoryFunction(llvm::Module &M) {
  auto &CTX = M.getContext();

  // 参数
  std::vector<llvm::Type *> TraceMemoryArgs{
      PointerType::getUnqual(Type::getInt8Ty(CTX)), Type::getInt64Ty(CTX),
      Type::getInt32Ty(CTX)};

  // 根据参数类型和返回类型（void）定义函数类型
  FunctionType *TraceMemoryTy =
      FunctionType::get(Type::getVoidTy(CTX), TraceMemoryArgs, false);

  // 模块中插入_TraceMemory函数
  FunctionCallee TraceMemory =
      M.getOrInsertFunction(TraceMemoryFunctionName, TraceMemoryTy);

  // dyn_cast 运行时的类型转换；
  llvm::Function *TraceMemoryFunction =
      dyn_cast<llvm::Function>(TraceMemory.getCallee());

  // 设置函数的链接属性为ExternalLinkage，表示该函数可能在其他模块中被引用
  TraceMemoryFunction->setLinkage(GlobalValue::ExternalLinkage);

  // 创建一个新的基本块，并将其插入到函数中作为函数的入口点
  llvm::BasicBlock *BB =
      llvm::BasicBlock::Create(CTX, "entry", TraceMemoryFunction);

  // 创建一个IR构建器，用于在这个基本块中插入指令
  IRBuilder<> Builder(BB);

  // 使用IR构建器创建一个ICmpNE指令，比较函数的第三个参数和0是否不相等，并将结果存储到CmpResult中
  // 比较TraceMemoryFunction函数的第3个参数是否与0相等
  llvm::Value *CmpResult =
      Builder.CreateICmpNE(TraceMemoryFunction->getArg(2), Builder.getInt32(0));

  // 创建两个新的基本块，分别用于处理比较结果为真和为假的情况

  llvm::BasicBlock *TraceLoadBB =
      llvm::BasicBlock::Create(CTX, "traceLoad", TraceMemoryFunction);

  llvm::BasicBlock *TraceStoreBB =
      llvm::BasicBlock::Create(CTX, "traceStore", TraceMemoryFunction);
  // 创建一个条件分支指令，根据CmpResult的值跳转到不同的基本块执行
  Builder.CreateCondBr(CmpResult, TraceLoadBB, TraceStoreBB);
  // 设置IR构建器的插入点为TraceLoadBB基本块的开始位置
  Builder.SetInsertPoint(TraceLoadBB);
  addMemoryTraceToBB(Builder, *TraceMemoryFunction, M, true);
  // 在当前插入点插入一个返回指令，返回void
  Builder.CreateRetVoid();

  // 设置IR构建器的插入点为TraceStoreBB基本块的开始位置
  Builder.SetInsertPoint(TraceStoreBB);
  addMemoryTraceToBB(Builder, *TraceMemoryFunction, M, false);
  Builder.CreateRetVoid();
}

void addMemoryTraceToInstruction(llvm::Module &M,llvm::Instruction &Instruction) {
    auto &CTX = M.getContext();

    std::vector<llvm::Type*> TraceMemoryArgs{
      PointerType::getUnqual(Type::getInt8Ty(CTX)),
      Type::getInt64Ty(CTX),
      Type::getInt32Ty(CTX)
    };


   FunctionType *TraceMemoryTy = FunctionType::get(Type::getVoidTy(CTX),TraceMemoryArgs,false);
   
   FunctionCallee TraceMemory = M.getOrInsertFunction(TraceMemoryFunctionName,TraceMemoryTy);

   IRBuilder<> Builder(Instruction.getNextNode());
   llvm::LoadInst *LoadInst = dyn_cast<llvm::LoadInst>(&Instruction);
   llvm::StoreInst *StoreInst =  dyn_cast<llvm::StoreInst>(&Instruction);

   llvm::Value *MemoryAddress;

   if(LoadInst){
      MemoryAddress = Builder.CreatePointerCast(LoadInst->getPointerOperand(),TraceMemoryArgs[0],"memoryAddress");
   } else {
      MemoryAddress = Builder.CreatePointerCast(StoreInst->getPointerOperand(),TraceMemoryArgs[0],"memoryAddress");
   }

   llvm::Value *CastTo64;
   llvm::Value *ValueToPrint = LoadInst ? &Instruction : StoreInst->getOperand(0);
   bool ShouldConvertPoiter =  ValueToPrint->getType()->isPointerTy();

   if(ShouldConvertPoiter){
    CastTo64 = Builder.CreatePtrToInt(ValueToPrint,TraceMemoryArgs[1],"castTo64");
   } else {
    CastTo64 = Builder.CreateIntCast(ValueToPrint,TraceMemoryArgs[1],false,"castTo64");
   }

   Builder.CreateCall(TraceMemory,{MemoryAddress,CastTo64,Builder.getInt32(Instruction.getOpcode() == llvm::Instruction::Load)});


   



}

struct MemoryTrace : public llvm::PassInfoMixin<MemoryTrace> {

  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
    // errs() << "In here\n\n";

    //  for(auto &F : M){
    //     dbgs() << "Function Name :" << F.getName() << "\n";
    //  }

    Function *main = M.getFunction("main");

    if (main) {
      addGlobalMemoryTraceFP(M);
      addMemoryTraceFPInitialization(M, *main);
      addTraceMemoryFunction(M);
      errs() << "Found main in module " << M.getName() << "\n";
      return llvm::PreservedAnalyses::none();
    } else {
      errs() << "Did not find main in " << M.getName() << "\n";
      return llvm::PreservedAnalyses::all();
    }


    for(llvm::Function &F : M) {
      if(F.getName() == TraceMemoryFunctionName) {
        continue;
      }
       
      for(llvm::BasicBlock &BB : F){
        for(llvm::Instruction &Instruction : BB){
          if(Instruction.getOpcode() == llvm::Instruction::Load ||
             Instruction.getOpcode() == llvm::Instruction::Store) {
              addMemoryTraceToInstruction(M,Instruction);
             }
        }
      } 
    }

    return llvm::PreservedAnalyses::all();
  }

  bool runOnModule(llvm::Module &M) { return true; }
};
} // namespace

llvm::PassPluginLibraryInfo getMemoryTracePluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "MemoryTrace", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "memory-trace") {
                    MPM.addPass(MemoryTrace());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getMemoryTracePluginInfo();
}
