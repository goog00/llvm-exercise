//=============================================================================
// FILE:
//    FindFCmpEq.cpp
//
// DESCRIPTION:
//    Visits all instructions in a function and returns all equality-based
//    floating point comparisons. The results can be printed through the use of
//    a printing pass.
//
//    This example demonstrates how to separate printing logic into a separate
//    printing pass, how to register it along with an analysis pass at the same
//    time, and how to parse pass pipeline elements to conditionally register a
//    pass. This is achieved using a combination of llvm::formatv() (not
//    strictly required),
//    llvm::PassBuilder::registerAnalysisRegistrationCallback(), and
//    llvm::PassBuilder::registerPipelineParsingCallback().
//
//    Originally developed for [1].
//
//    [1] "Writing an LLVM Optimization" by Jonathan Smith
//
// USAGE:
//      opt --load-pass-plugin libFindFCmpEq.dylib `\`
//        --passes='print<find-fcmp-eq>' --disable-output <input-llvm-file>
//
// License: MIT

// 访问函数中的所有指令，并返回所有基于等式的浮点比较。结果可以通过使用打印pass来打印。这个例子展示了如何将打印逻辑分离到单独的打印pass，如何在同一时间注册它与分析pass，以及如何解析pass管道元素以条件注册pass。
//这是通过结合使用llvm::formatv()（非必需）、llvm::PassBuilder::registerAnalysisRegistrationCallback()
// 和 llvm::PassBuilder::registerPipelineParsingCallback() 来实现的。 

// 表述了一个程序或算法会对函数内的所有指令进行遍历，并对这些指令进行浮点数比较，如果这些指令的运算结果符合等式条件，则会被返回。同时，这个程序或算法还具有打印这些比较结果的功能。
// 此外，这段代码还展示了如何将打印逻辑分离出来，如何同时注册一个分析pass，以及如何解析pass管道元素以条件注册一个pass。这是通过使用特定的编程方法来实现的，这些方法并非必需，但可以帮助代码更加清晰易读。最后，这段代码最初是为某个特定的目的或项目（引用
// [1]）开发的。
//=============================================================================

#include "FindFCmpEq.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"

#include <string>

using namespace llvm;

namespace {
static void
printFCmpEqInstructions(raw_ostream &OS, Function &Func,
                        const FindFCmpEq::Result &FCmpEqInsts) noexcept {
  if (FCmpEqInsts.empty())
    return;

  OS << "Floating-point equality comparisons in \"" << Func.getName()
     << "\" : \n";

  // 使用 ModuleSlotTracker
  // 进行打印使得插槽编号的完整功能分析仅发生一次，而不是每次打印指令时发生。
  ModuleSlotTracker Tracker(Func.getParent());

  for (FCmpInst *FCmpEq : FCmpEqInsts) {
    FCmpEq->print(OS, Tracker);
    OS << '\n';
  }
}

} // namespace

// FindFCmpEq implementation
FindFCmpEq::Result FindFCmpEq::run(Function &Func,
                                   FunctionAnalysisManager &FAM) {
  return run(Func);
}

FindFCmpEq::Result FindFCmpEq::run(Function &Func) {
  Result Comparisons;
  for (Instruction &Inst : instructions(Func)) {
    if (auto *FCmp = dyn_cast<FCmpInst>(&Inst)) {
      if (FCmp->isEquality()) {
        Comparisons.push_back(FCmp);
      }
    }
  }

  return Comparisons;
}

PreservedAnalyses FindFCmpEqPrinter::run(Function &Func,
                                         FunctionAnalysisManager &FAM) {
  auto &Comparisons = FAM.getResult<FindFCmpEq>(Func);
  printFCmpEqInstructions(OS, Func, Comparisons);
  return PreservedAnalyses::all();
}

static constexpr char PassArg[] = "find-fcmp-eq";
static constexpr char PassName[] = "Floating-point equality comparions locator";
static constexpr char PluginName[] = "FindFCmpEq";

// New PM Registration
llvm::AnalysisKey FindFCmpEq::Key;

PassPluginLibraryInfo getFindFCmpEqPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, PluginName, LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            // #
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([&] { return FindFCmpEq(); });
                });
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  std::string PrinterPassElement =
                      formatv("print<{0}>", PassArg);
                  if (Name.equals(PrinterPassElement)) {
                    FPM.addPass(FindFCmpEqPrinter(llvm::outs()));
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getFindFCmpEqPluginInfo();
}