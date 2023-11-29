#ifndef LLVM_EXERCISE_OPCODECOUNTER_H
#define LLVM_EXERCISE_OPCODECOUNTER_H

#include "llvm/ADT/StringMap.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

// New PM Interface

// using的一种用法，定义StringMap类型的别名
using ResultOpcodeCounter = llvm::StringMap<unsigned>;

struct OpcodeCounter : public llvm::AnalysisInfoMixin<OpcodeCounter> {
  using Result = ResultOpcodeCounter;
  //run方法有两个引用参数
  Result run(llvm::Function &F, llvm::FunctionAnalysisManager &);

  OpcodeCounter::Result generateOpcodeMap(llvm::Function &F);

  static bool isRequired() { return true; }

private:
  static llvm::AnalysisKey Key;
  friend struct llvm::AnalysisInfoMixin<OpcodeCounter>;
};

// New PM interface for the printer pass
// OpcodeCounterPrinter 继承 llvm::PassInfoMixin
class OpcodeCounterPrinter : public llvm::PassInfoMixin<OpcodeCounterPrinter> {
public:

  //在C++中,explicit表示构造函数或析构函数是显式声明的，也就是说它们不会被编译器自动生成
  //构造函数初始化列表以一个冒号开始，接着是以逗号分隔的数据成员列表，
  //每个数据成员后面跟一个放在括号中的初始化式
  explicit OpcodeCounterPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}
  llvm::PreservedAnalyses run(llvm::Function &Func,
                              llvm::FunctionAnalysisManager &FAM);
  static bool isRequired() { return true; }

private:
  llvm::raw_ostream &OS;

};

#endif