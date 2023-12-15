#include <llvm/IR/Constant.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Module.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace {
class TransformPass final : public PassInfoMixin<TransformPass> {
private:
  bool runOnBasicBlock(BasicBlock &B) {
    // 获取第一条指令和第二条指令
    Instruction &Inst1st = *B.begin(), &Inst2nd = *(++B.begin());

    // 断言第一条指令的地址是否等于第二条指令的第一个操作数的地址
    assert(&Inst1st == Inst2nd.getOperand(0));

    // 打印第一条指令
    outs() << "I am the first instruction:" << Inst1st << "\n";

    // 把第一条指令当作为操作数
    outs() << "Me as an operand:";
    Inst1st.printAsOperand(outs(), false);
    outs() << "\n";

    // User-Use-Value
// %add = add i32 %a, %b
// 这个指令表示将两个整数 %a 和 %b 相加，并将结果存储在 %add 中。在这个例子中：
// Values（值）: %a, %b, 和 %add 都是 Value 的实例。在 LLVM 中，它们是数据的表示，可以是指令的结果，也可以是用作指令的输入。
// User（使用者）: add 指令是一个 User，因为它使用了其他 Value 实例（即 %a 和 %b）作为它的操作数。
// Use（使用）: %a 和 %b 在 add 指令中的使用被表示为 Use 对象。每个 Use 对象链接一个 User（这里是 add 指令）和一个被使用的 Value（%a 或 %b）
    outs() << "My Operands:\n";

    for (auto *Iter = Inst1st.op_begin(); Iter != Inst1st.op_end(); ++Iter) {
      Value *Operand = *Iter;

      if (Argument *Arg = dyn_cast<Argument>(Operand)) {
        outs() << "I am function " << Arg->getParent()->getName() << "\'s #"
               << Arg->getArgNo() << " argument"
               << "\n";
      }

      if (ConstantInt *C = dyn_cast<ConstantInt>(Operand)) {
        outs() << "I am a constant integer of value " << C->getValue() << "\n";
      }
    }

    outs() << "My users:\n";

    for (auto Iter = Inst1st.user_begin(); Iter != Inst1st.user_end(); ++Iter) {
      outs() << "\t" << *(dyn_cast<Instruction>(*Iter)) << "\n";
    }

    outs() << "My uses (same with users): \n";
    for (auto Iter = Inst1st.use_begin(); Iter != Inst1st.use_end(); ++Iter) {
      outs() << "\t" << *(dyn_cast<Instruction>(Iter->getUser())) << "\n";
    }

    // 指令维护

    Instruction *NewInst = BinaryOperator::Create(
        Instruction::Add, Inst1st.getOperand(0), Inst1st.getOperand(0));

    NewInst->insertAfter(&Inst1st);

    // Two Questions for you to answer:
    //
    // (1) Is there any alternative to updating each reference separately?
    //     Please check the documentation and try answering this.
    //
    // (2) What happens if we update the use references without the insertion?

    Inst1st.user_begin()->setOperand(0, NewInst);

    return true;
  }

  bool runOnFunction(Function &F) {
    bool Transformed = false;

    for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
      if (runOnBasicBlock(*Iter)) {
        Transformed = true;
      }
    }
    return Transformed;
  }

public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    for (auto Iter = M.begin(); Iter != M.end(); ++Iter) {
      if (runOnFunction(*Iter)) {
        return PreservedAnalyses::none();
      }
    }
    return PreservedAnalyses::all();
  }
};

} // anonymous namespace




extern "C" PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {
      .APIVersion = LLVM_PLUGIN_API_VERSION,
      .PluginName = "FunctionInfo",
      .PluginVersion = LLVM_VERSION_STRING,
      .RegisterPassBuilderCallbacks =
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) -> bool {
                  if (Name == "transform") {
                    MPM.addPass(TransformPass());
                    return true;
                  }
                  return false;
                });
          } // RegisterPassBuilderCallbacks
  };        // struct PassPluginLibraryInfo
}