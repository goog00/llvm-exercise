//===- examples/ModuleMaker/ModuleMaker.cpp - Example project ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This programs is a simple example that creates an LLVM module "from scratch",
// emitting it as a bitcode file to standard out.  This is just to show how
// LLVM projects work and to demonstrate some of the LLVM APIs.
//
//===----------------------------------------------------------------------===//


#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"


using namespace llvm;

int main() {
    LLVMContext Context;


    //创建Module
    Module *M = new Module("test",Context);
 
     // 创建主函数：首先创建类型 'int ()'
    FunctionType *FT = FunctionType::get(Type::getInt32Ty(Context),/*无参*/false);

    //Function的构造函数的最后一个参数为Module，函数自动跟在Module之后
    Function *F = Function::Create(FT,Function::ExternalLinkage,"main",M);
    
    //在函数中添加基本块，
    BasicBlock *BB = BasicBlock::Create(Context,"EntryBlock",F);


   //构造常量，返回常量的指针
    Value *Two = ConstantInt::get(Type::getInt32Ty(Context),2);
    Value *Three =  ConstantInt::get(Type::getInt32Ty(Context),3);

    //创建add 指令
    Instruction *Add = BinaryOperator::Create(Instruction::Add,Two,Three,"addresult");

    //add指令明确插入bb
    Add->insertInto(BB,BB->end());

    //在BB，创建return指令
    ReturnInst::Create(Context,Add)->insertInto(BB,BB->end());

    //输出二进制文件
    WriteBitcodeToFile(*M,outs());

    delete M;

    return  0;










}