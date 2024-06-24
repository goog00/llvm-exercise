#include "InjectFuncCall.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

#define DEBUG_TYPE "inject-func-call"


//这个函数中包括几个开发中常用的功能：
//1.在模块中声明函数，如声明了printf函数
//2.创建全局变量并初始化，如创建了全局变量PrintfFormatStrVar
//3.遍历每个函数，并在其入口处插入printf调用，打印函数名称和参数数量

bool InjectFuncCall::runOnModule(Module &M) {

    //用于跟踪是否至少插入异常’printf'调用
    bool InsertedAtLeastOnePrintf = false;

    //模块的上下文，用于创建LLVM类型和变量
    auto &CTX = M.getContext();

    //PrintArgTy 是一个指向‘i8'类型的指针，即char* 类型
    PointerType *PrintfArgTy = PointerType::getUnqual(Type::getInt8Ty(CTX));

    //声明printf 函数

    //定义printf 函数类型，其返回i32 并接受一个char*和可变数量的参数
    FunctionType *PrintfTy = FunctionType::get(IntegerType::getInt32Ty(CTX),
                            PrintfArgTy,
                            /*ISVarArs=*/true);

    //使用getOrInsertFunction 在模块中插入 printf 函数声明
    FunctionCallee Printf = M.getOrInsertFunction("printf",PrintfTy);

    //为函数设置一些属性，例如不抛异常、参数0不捕获和只读
    Function *PrintfF = dyn_cast<Function>(Printf.getCallee());
    PrintfF->setDoesNotThrow();
    PrintfF->addParamAttr(0,Attribute::NoCapture);
    PrintfF->addParamAttr(0,Attribute::ReadOnly);

    //插入格式字符串全局变量
    //创建一个包含格式字符串的常量数组
    llvm::ConstantDataArray::getString(CTX,"(llvm-tutor)Hello from:%s\n(llvm-tutor)  number of arguments: %d\n");

    //在模块中插入一个全局变量以保存这个格式字符串，并设置其初始值
    Constant *PrintfFormatStrVar = M.getOrInsertGlobal("PrintfFormatStr",PrintfFormatStr->getType());
    dyn_cast<GlobalVariable>(PrintfFormatStrVar)->setInitializer(PrintfFormatStr);

    for(auto &F : M) {
        if(F.isDeclaration())
           continue;

        //使用 IRBuilder 在函数的入口处设置插入点。
        IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());

        // 为函数名创建一个全局字符串指针。
        auto FuncName = Builder.CreateGlobalStringPtr(F.getName());   

        //将格式字符串全局变量转换为 char* 类型
       llvm::Value *FormatStrPtr =  Builder.CreatePointerCast(PrintfFormatStrVar,PrintfArgTy,"formatStr");

        LLVM_DEBUG(dbgs() << " Injecting call to printf inside " << F.getName()
                    << "\n");

        //创建并插入printf调用，传递格式字符串、函数名和参数数量         
        Builder.CreateCall(Printf,{FormatStrPtr,FuncName,Builder.getInt32(F.arg_size())});

        //设置 InsertedAtLeastOnePrintf 为 true，表示至少插入了一次 printf 调用。
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