#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace {
    class FunctionInfoPass final : public PassInfoMixin<FunctionInfoPass> {
        public:
           PreservedAnalyses run([[maybe_unused]] Module &M, ModuleAnalysisManager &) {
            outs() << "csc d70 function information pass";
            
           for(auto iter = M.begin(); iter != M.end(); ++iter){
            Function &func = *iter;
            outs() << "Name:" << func.getName() << "\n";
            outs() << "Number of Arguments: " << func.arg_size() << "\n";
            outs() << "Number of Direct Call Sites in the same llvm module:"
                   << func.getNumUses() << "\n";
            outs() << "Number of Basic Blocks: " << func.size() << "\n";
            outs() << "Number of Instructions:"  << func.getInstructionCount() << "\n";       

           }

            return PreservedAnalyses::all();
        }
    };//class FunctionInfoPass


}// anonymous namespace 

extern "C" PassPluginLibraryInfo  llvmGetPassPluginInfo() {
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "FunctionInfo",
        .PluginVersion = LLVM_VERSION_STRING,
        .RegisterPassBuilderCallbacks = 
           [](PassBuilder &PB){
                PB.registerPipelineParsingCallback(
                    [](StringRef Name,ModulePassManager &MPM,
                        ArrayRef<PassBuilder::PipelineElement>)->bool {

                            if(Name == "function-info"){
                                MPM.addPass(FunctionInfoPass());
                                return true;
                            } 
                            return false;

                    }
                );
           }
    };
}


 