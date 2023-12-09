#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// 定义一个常量字符串，命名为"_MemoryTraceFP"，用作全局变量的名称。
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

void addMemoryTraceFPInitialization(llvm::Module &M, llvm::Function &MainFunc) {
  auto &CTX = M.getContext();

  std::vector<llvm::Type *> FopenArgs{
      PointerType::getUnqual(Type::getInt8Ty(CTX)),
      PointerType::getUnqual(Type::getInt8Ty(CTX))};

  FunctionType *FopenTy = FunctionType::get(
      PointerType::getUnqual(Type::getInt8Ty(CTX)), FopenArgs, false);

  FunctionCallee Fopen = M.getOrInsertFunction("fopen", FopenTy);

  Constant *FopenFileNameStr =
      llvm::ConstantDataArray::getString(CTX, "memory-traces.log");
  Constant *FopenFileNameStrVar =
      M.getOrInsertGlobal("FopenFileNameStr", FopenFileNameStr->getType());
  dyn_cast<GlobalVariable>(FopenFileNameStrVar)
      ->setInitializer(FopenFileNameStr);

  Constant *FopenModeStr = llvm::ConstantDataArray::getString(CTX, "w+");
  Constant *FopenModeStrVar =
      M.getOrInsertGlobal("FopenModeStr", FopenModeStr->getType());
  dyn_cast<GlobalVariable>(FopenModeStrVar)->setInitializer(FopenModeStr);

  IRBuilder<> Builder(&*MainFunc.getEntryBlock().getFirstInsertionPt());
  llvm::Value *FopenFileNameStrPtr = Builder.CreatePointerCast(
      FopenFileNameStrVar, FopenArgs[0], "fileNameStr");

  llvm::Value *FopenModeStrPtr =
      Builder.CreatePointerCast(FopenModeStrVar, FopenArgs[0], "modelStr");

  llvm::Value *FopenReturn =
      Builder.CreateCall(Fopen, {FopenFileNameStrPtr, FopenModeStrPtr});

  GlobalVariable *FPGobal = M.getNamedGlobal(FilePointerVarName);
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
      errs() << "Found main in module " << M.getName() << "\n";
      return llvm::PreservedAnalyses::none();
    } else {
      errs() << "Did not find main in " << M.getName() << "\n";
      return llvm::PreservedAnalyses::all();
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
