#include "InjectFuncCall.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

#define DEBUG_TYPE "inject-func-call"

bool InjectFuncCall::runOnModule(Module &M) {
  bool InsertedAtLeastOnePrintf = false;

  auto &CTX = M.getContext();

  PointerType *PrintfArgTy = PointerType::getUnqual(Type::getInt8Ty(CTX));

  FunctionType *PrintfTy =
      FunctionType::get(IntegerType::getInt32Ty(CTX), PrintfArgTy, true);

  FunctionCallee Printf = M.getOrInsertFunction("printf", PrintfTy);

  //
  Function *PrintfF = dyn_cast<Function>(Printf.getCallee());
  PrintfF->setDoesNotThrow();
  PrintfF->addParamAttr(0, Attribute::NoCapture);
  PrintfF->addParamAttr(0, Attribute::ReadOnly);

  llvm::Constant *PrintfFormatStr = llvm::ConstantDataArray::getString(
      CTX, "(llvm-exercise) Hello from :%s\n(llvm-exercise)  number of "
           "arguments: %d\n");

  Constant *PrintfFormatStrVal =
      M.getOrInsertGlobal("PrintfFormatStr", PrintfFormatStr->getType());

  dyn_cast<GlobalVariable>(PrintfFormatStrVal)->setInitializer(PrintfFormatStr);

  // step 3 : for each function in the module, inject a call to printf
  for (auto &F : M) {
    if (F.isDeclaration())
      continue;

    // get an IR builder. sets the insertion point to the top of the function
    IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());

    // inject a global variable that contains the function name
    auto FuncName = Builder.CreateGlobalStringPtr(F.getName());

    //
    llvm::Value *FormatStrPtr =
        Builder.CreatePointerCast(PrintfFormatStrVal, PrintfArgTy, "formatStr");

      LLVM_DEBUG(dbgs() << " Injecting call to printf inside " << F.getName()
                      << "\n");


     Builder.CreateCall(Printf,{FormatStrPtr,FuncName,Builder.getInt32(F.arg_size())});

     InsertedAtLeastOnePrintf = true;                  
  }

  return InsertedAtLeastOnePrintf;
}


PreservedAnalyses InjectFuncCall::run(llvm::Module &M,
                                      llvm::ModuleAnalysisManager &) {
        
  bool Changed = runOnModule(M);

  return (Changed ? llvm::PreservedAnalyses::none() : llvm::PreservedAnalyses::all());
}



//new Pm registration

llvm::PassPluginLibraryInfo getInjectFuncCallPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION,"inject-func-call",LLVM_VERSION_STRING,
            [](PassBuilder &PB){
                PB.registerPipelineParsingCallback(
                    [](StringRef Name,ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>){
                        if(Name == "inject-func-call"){
                            MPM.addPass(InjectFuncCall());
                            return true;
                        }
                        return false;
                    }
                );
            }

    };
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo(){
    return getInjectFuncCallPluginInfo();
}