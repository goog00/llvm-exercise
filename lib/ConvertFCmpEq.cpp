//=============================================================================
// FILE:
//    ConvertFCmpEq.cpp
//
// DESCRIPTION:
//    Transformation pass which uses the results of the FindFCmpEq analysis pass
//    to convert all equality-based floating point comparison instructions in a
//    function to indirect, difference-based comparisons.
//
//    This example demonstrates how to couple an analysis pass with a
//    transformation pass, the use of statistics (the STATISTIC macro), and LLVM
//    debugging operations (the LLVM_DEBUG macro and the llvm::dbgs() output
//    stream). It also demonstrates how instructions can be modified without
//    having to completely replace them.
//
//    Originally developed for [1].
//
//    [1] "Writing an LLVM Optimization" by Jonathan Smith
//
// USAGE:
//      opt --load-pass-plugin libConvertFCmpEq.dylib [--stats] `\`
//        --passes='convert-fcmp-eq' --disable-output <input-llvm-file>
//
// License: MIT
//=============================================================================

#include "ConvertFCmpEq.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include <cassert>

using namespace llvm;

namespace {
FCmpInst *convertFCmpEqInstruction(FCmpInst *FCmp) noexcept {
  assert(FCmp && "The given fcmp instruction is null");

  // 判断给定的 FCmpInst 对象是否表示一个等式比较。如果不是，函数会返回 null。
  if (!FCmp->isEquality()) {
    return nullptr;
  }

  Value *LHS = FCmp->getOperand(0);
  Value *RHS = FCmp->getOperand(1);

  //// 根据当前比较谓词确定新的浮点比较谓词。

  CmpInst::Predicate CmpPred = [FCmp] {
    switch (FCmp->getPredicate()) {
    case CmpInst::Predicate::FCMP_OEQ:
      return CmpInst::Predicate::FCMP_OLT;
    case CmpInst::Predicate::FCMP_UEQ:
      return CmpInst::Predicate::FCMP_ULT;
    case CmpInst::Predicate::FCMP_ONE:
      return CmpInst::Predicate::FCMP_OGE;
    case CmpInst::Predicate::FCMP_UNE:
      return CmpInst::Predicate::FCMP_UGE;
    default:
      llvm_unreachable("Unsupported fcmp predicate");
    }
  }();

  // 将一个浮点数比较操作转换为等式比较操作，这通常用于优化某些特定的计算或算法。

  Module *M = FCmp->getModule();
  assert(M && "The given fcmp instruction does not belong to a module");

  LLVMContext &Ctx = M->getContext();
  // 定义了一个64位的整数类型
  IntegerType *I64Ty = IntegerType::get(Ctx, 64);
  // 定义了一个双精度浮点数类型。
  Type *DoubleTy = Type::getDoubleTy(Ctx);

  // 这段代码是使用C++编写的，它定义了一个名为SignMask的常量整数，该整数是64位整数类型I64Ty的一个实例，
  // 其值为~(1L<<
  // 63)。这个表达式产生一个所有位都是1，除了最高位（符号位）是0的64位整数。

  // 1L << 63
  // 是一个将1左移63位的操作。这产生了一个只有最高位是1，其他位都是0的64位整数。
  // ~(1L << 63)
  // 是一个按位取反操作，它将上述整数的所有1变为0，所有0变为1。这产生了一个只有符号位是0，其他位都是1的64位整数。
  // 这样的整数在计算机科学中有许多用途，特别是在处理二进制补码表示法和浮点数比较时。在二进制补码表示法中，负数的最高位（符号位）是1，而正数和零的最高位是0。
  // 因此，这个常数可以用来表示一个64位负数的最大值（即INT64_MIN）。
  // 关于"double-precision machine epsilon"常量，你的代码片段中并未给出定义。
  // 这通常是在计算机图形学、数值分析和相关领域中使用的常量，表示浮点数精度的限制。

  // 符号掩码（sign-mask);符号掩码是一个所有位都是1，除了最高位（符号位）是0的64位整数
  ConstantInt *SignMask = ConstantInt::get(I64Ty, ~(1L << 63));
  // 双精度机器epsilon常数;双精度机器epsilon常数是根据IEEE
  // 754双精度值的标准定义的，用于表示浮点数的精度限制。
  APInt EpsilonBits(64, 0x3CB0000000000000);

  Constant *EpsilonValue =
      ConstantFP::get(DoubleTy, EpsilonBits.bitsToDouble());

  // 创建一个 IRBuilder，插入点设置为给定的 fcmp 指令。
  IRBuilder<> Builder(FCmp);

  // 同时创建减法 ，转型 绝对值 和新的比较指令

  // 减法指令 : 将两个浮点数相减。
  //%0 = fsub double %a ,%b
  auto *FSubInst = Builder.CreateFSub(LHS, RHS);

  // 转型指令 :将一个类型的值转换为另一个类型。
  //  %1 = bitcast double %0 to i64
  auto *CastToI64 = Builder.CreateBitCast(FSubInst, I64Ty);

  // 对两个整数进行按位与操作
  //%2 = and i64 %1 , 0x7fffffffffffffff
  auto *AbsValue = Builder.CreateAnd(CastToI64, SignMask);

  // 对两个浮点数进行比较。
  //%3 = bitcast i64 %2 to double
  auto *CastToDouble = Builder.CreateBitCast(AbsValue, DoubleTy);

  // 代码更改了现有fcmp指令的谓词和操作数。
  // 这行代码没有创建一个新的fcmp指令，而是改变了现有fcmp指令的谓词和操作数，以匹配我们想要进行的比较。
  FCmp->setPredicate(CmpPred);
  FCmp->setOperand(0, CastToDouble);
  FCmp->setOperand(1, EpsilonValue);
  return FCmp;
}
} // namespace


static constexpr char PassArg[] = "convert-fcmp-eq";
static constexpr char PassName[] =
    "Convert floating-point equality comparisons";
static constexpr char PluginName[] = "ConvertFCmpEq";



#define DEBUG_TYPE ::PassArg
STATISTIC(FCmpEqConversionCount,
          "Number of direct floating-point equality comparisons converted");

//------------------------------------------------------------------------------
// ConvertFCmpEq implementation
//------------------------------------------------------------------------------
PreservedAnalyses ConvertFCmpEq::run(Function &Func,
                                     FunctionAnalysisManager &FAM) {
  auto &Comparisons = FAM.getResult<FindFCmpEq>(Func);
  bool Modified = run(Func, Comparisons);
  return Modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

bool ConvertFCmpEq::run(llvm::Function &Func,
                        const FindFCmpEq::Result &Comparisons) {
  bool Modified = false;
  // Functions marked explicitly 'optnone' should be ignored since we shouldn't
  // be changing anything in them anyway.
  if (Func.hasFnAttribute(Attribute::OptimizeNone)) {
    LLVM_DEBUG(dbgs() << "Ignoring optnone-marked function \"" << Func.getName()
                      << "\"\n");
    Modified = false;
  } else {
    for (FCmpInst *FCmp : Comparisons) {
      if (convertFCmpEqInstruction(FCmp)) {
        ++FCmpEqConversionCount;
        Modified = true;
      }
    }
  }

  return Modified;
}          

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
PassPluginLibraryInfo getConvertFCmpEqPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, PluginName, LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name.equals(PassArg)) {
                    FPM.addPass(ConvertFCmpEq());
                    return true;
                  }

                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getConvertFCmpEqPluginInfo();
}
