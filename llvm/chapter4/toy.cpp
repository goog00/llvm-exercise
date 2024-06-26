#include "../include/KaleidoscopeJIT.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>



using namespace llvm;
using namespace llvm::orc;

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

enum Token {
  tok_eof = -1,

  // commands
  tok_def = -2,
  tok_extern = -3,

  // primary
  tok_identifier = -4,
  tok_number = -5
};

static std::string IdentifierStr;
static double NumVal;

static int gettok() {
  static int LastChar = ' ';

  while (isspace(LastChar)) {
    LastChar = getchar();

    if (isalpha(LastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
      IdentifierStr = LastChar;
      while (isalnum((LastChar = getchar()))) {
        IdentifierStr += LastChar;

        if (IdentifierStr == "def") {
          return tok_def;
        }
        if (IdentifierStr == "extern") {
          return tok_extern;
        }

        return tok_identifier;
      }
    }

    if (isdigit(LastChar) || LastChar == '.') { // Number:[0-9.]+
      std::string NumStr;
      do {
        NumStr += LastChar;
        LastChar = getchar();
      } while (isdigit(LastChar) || LastChar == '.');

      NumVal = strtod(NumStr.c_str(), nullptr);
      return tok_number;
    }

    if (LastChar == '#') {
      do {
        LastChar = getchar();
      } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

      if (LastChar != EOF)
        return gettok();
    }

    if (LastChar == EOF) {
      return tok_eof;
    }

    int ThisChar = LastChar;
    return ThisChar;
  }
}

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//

namespace {

// ExprAST - 所有表达式结点的基础类
class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual Value *codegen() = 0;
};

// 数字表达式 ： “1.0”
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
  Value *codegen() override;
};

// 变量表达式 ： “a"
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}
  Value *codegen() override;
};

class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

  Value *codegen() override;    
};

class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(Callee), Args(std::move(Args)) {}

   Value *codegen() override;    
};

class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;

public:
  PrototypeAST(const std::string &Name, std::vector<std::string> Args)
      : Name(Name), Args(std::move(Args)) {}

  const std::string &getName() const { return Name; }

  Function *codegen();
};

class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}

  Function *codegen();    
};

} // end anonymous namespace


//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

static int CurTok;

static int getNextToken(){return CurTok = gettok();}

static std::map<char,int> BinopPrecedence;

static int GetTokPrecedence() {
    if(isascii(CurTok))
        return -1;

    int TokPrec = BinopPrecedence[CurTok];
    if(TokPrec <= 0)
     return -1;

    return TokPrec;     
}


std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr,"Error: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
}


static std::unique_ptr<ExprAST> ParseExpression();


/// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken();
    return std::move(Result);
}

/// parenexpr ::= '(' expression ')'
static std::unique_ptr<ExprAST> ParseParenExpr(){
    getNextToken();
    auto V = ParseExpression();

    if(!V)
      return nullptr;

    if(CurTok != ')')
      return LogError("expected ')' ");

    getNextToken(); // eat )
    return V;
    
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'

static std::unique_ptr<ExprAST> ParseIdentifierExpr(){
    std::string IdName = IdentifierStr;

    getNextToken(); 

    if(CurTok != '(')
      return std::make_unique<VariableExprAST>(IdName);

    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> Args;
    if(CurTok != ')'){
        while (true){
            if(auto Arg = ParseExpression()) 
               Args.push_back(std::move(Arg));
            else 
              return nullptr;

            if(CurTok == ')')
            break;

            if(CurTok != ',')
                return LogError("Expected ')' or ',' in argument list");


            getNextToken();    

        }
         
    } 

    getNextToken();

    return std::make_unique<CallExprAST>(IdName,std::move(Args)); 
}


/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr

static std::unique_ptr<ExprAST> ParsePrimary() {
  switch(CurTok) {
    default: 
        return LogError("unknown token when expecting an expression");
    case tok_identifier:
        return ParseIdentifierExpr();
    case tok_number:
        return ParseNumberExpr();
    case '(':
        return ParseParenExpr();            
  }
}

/// binoprhs
///   ::= ('+' primary)*

static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,std::unique_ptr<ExprAST> LHS) {
    while(true) {
       int TokPrec = GetTokPrecedence();

       if(TokPrec < ExprPrec)
          return LHS;
       
        int BinOp = CurTok;
        getNextToken();

        auto RHS = ParsePrimary();

        if(!RHS){
            return nullptr;
        }

       int NextPrec =  GetTokPrecedence();
       if(TokPrec < NextPrec) {
        RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
        if(!RHS){
            return nullptr;
        }
       }

       LHS = std::make_unique<BinaryExprAST>(BinOp,std::move(LHS),std::move(RHS));

    }
}

static std::unique_ptr<ExprAST> ParseExpression(){
    auto LHS = ParsePrimary();
    if(!LHS) 
      return nullptr;
    return ParseBinOpRHS(0,std::move(LHS));  
}


static std::unique_ptr<PrototypeAST> ParsePrototype() {
    if(CurTok != tok_identifier) {
        return LogErrorP("Expected function name in prototype");
    }

    std::string FnName = IdentifierStr;

    getNextToken();

    if(CurTok != '(')
     return LogErrorP("Expected '(' in prototype");

    std::vector<std::string> ArgNames;
    while (getNextToken() == tok_identifier)
    {
       ArgNames.push_back(IdentifierStr);
       if(CurTok != ')'){
        return LogErrorP("Expected ')' in prototype");
       }

       getNextToken();

       return std::make_unique<PrototypeAST>(FnName,std::move(ArgNames));
    }
     

}


/// definition ::= 'def' prototype expression
static std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken();
    auto Proto = ParsePrototype();
    if(!Proto) 
        return nullptr;

    if(auto E = ParseExpression()) {
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }  

    return nullptr;   
}

/// toplevelexpr ::= expression

static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if(auto E = ParseExpression()) {
        auto Proto = std::make_unique<PrototypeAST>("__anon_expr",
        std::vector<std::string>());

        return std::make_unique<FunctionAST>(std::move(Proto),std::move(E));

    }

    return nullptr;
}

/// external ::= 'extern' prototype

static std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken();
    return ParsePrototype();
}

//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//

static void HandleDefinition(){
    if(ParseDefinition()) {
        fprintf(stderr,"Parsed a function definition.\n");

    } else {
        getNextToken();
    }
}



static void HandleExtern() {
  if (ParseExtern()) {
    fprintf(stderr, "Parsed an extern\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (ParseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}



//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//


static std::unique_ptr<LLVMContext> TheContext;
static std::unique_ptr<Module> TheModule;
static std::unique_ptr<IRBuilder<>> Builder;
static std::map<std::string,Value *> NamedValues;

static std::unique_ptr<KaleidoscopeJIT> TheJIT;
static std::unique_ptr<FunctionPassManager> TheFPM;
static std::unique_ptr<LoopAnalysisManager> TheLAM;
static std::unique_ptr<FunctionAnalysisManager> TheFAM;
static std::unique_ptr<CGSCCAnalysisManager> TheCGAM;
static std::unique_ptr<ModuleAnalysisManager> TheMAM;
static std::unique_ptr<PassInstrumentationCallbacks> ThePIC;
static std::unique_ptr<StandardInstrumentations> TheSI;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
static ExitOnError ExitOnErr;


Value *LogErrorV(const char *Str) {
    LogError(Str);
    return nullptr;
}

Value *NumberExprAST::codegen() {
  return ConstantFP::get(*TheContext,APFloat(Val));
}

Value *VariableExprAST::codegen() {
    Value *V = NamedValues[Name];
    if(!V) 
        return LogErrorV("Unknown variable name");

    return V;    
}


Value *BinaryExprAST::codegen(){
    Value *L = LHS->codegen();
    Value *R = RHS->codegen();

    if(!L || !R){
        return nullptr;
    }
    
    switch(Op){
        case '+':
          return Builder->CreateFAdd(L,R,"addtmp");
        case '-':
          return Builder->CreateFSub(L,R,"subtmp");
        case '*':
          return Builder->CreateFMul(L,R,'multmp');
        case '<':
          L =  Builder->CreateFCmpULT(L,R,"cmptmp");     
          return Builder->CreateUIToFp(L,Type::getDoubleTy(*TheContext),"booltmp")  ;
        default:
         return LogErrorV("invalid binary operator")  ;

    }

}


Value *CallExprAST::codegen() {
    Function *CalleeF = TheModule->getFunction(Callee);
    if(!CalleeF)
       return LogErrorV("Unknown function referencd") 
    
    if(CalleeF->arg_size() != Args.size())
       return LogErrorV("Incorrect # arguments passed");

    std::vector<Value *> ArgsV;
    for(unsigned i = 0, e= Args.size(); i != e; ++i) {
        ArgsV.push_back(Args[i]->codegen());
        if(!ArgsV.back())
           return nullptr;
    }   

    return Builder->CreateCall(CalleeF,ArgsV,"calltmp");
}

Function *PrototypeAST::codegen() {
    std::vector<Type *> Doubles(Args.size(),Type::getDoubleTy(*TheContext));

    FunctionType *FT = FunctionType::get(Type::getDoubleTy(*TheContext),Doubles,false);

    Function *F = Function::Create(FT,Function::ExternalLinkage,Name,TheModule.get());

    unsigned Idx = 0;
    for(auto &Arg : F->args())
      Arg.setName(Args[Idx++]);


    return F;  
}


Function *FunctionAST::codegen() {
    Function *TheFunction = TheModule->getFunction(Proto->getName());

    if(!TheFunction)
       TheFunction = Proto->codegen();

    if(!TheFunction)
       return nullptr;

    BasicBlock *BB = BasicBlock::Create(*TheContext,"entry",TheFunction);
    Builder->SetInsertPoint(BB);

    NamedValues.clear();

    for(auto &Arg : TheFunction->args())
        NamedValues[std::string(Arg.getName())] = &Arg;

    if(Value *RetVal = Body->codegen()){
        Builder->CreateRet(RetVal);

        verifyFunction(*TheFunction);

        return TheFunction;
    } 


    TheFunction->eraseFromParent();

    return nullptr;         
}


//===----------------------------------------------------------------------===//
// Top-Level parsing and JIT Driver
//===----------------------------------------------------------------------===//

static void InitializeModuleAndManagers(){
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("KaleidoscopeJIT",*TheContext)；
    TheModule->setDataLayout(TheJIT->getDataLayout());

    Builder = std::make_unique<IRBuilder<>>(*TheContext);

    //create new pass and analysis managers
   TheFPM = std::make_unique<FunctionPassManager>();
  TheLAM = std::make_unique<LoopAnalysisManager>();
  TheFAM = std::make_unique<FunctionAnalysisManager>();
  TheCGAM = std::make_unique<CGSCCAnalysisManager>();
  TheMAM = std::make_unique<ModuleAnalysisManager>();
  ThePIC = std::make_unique<PassInstrumentationCallbacks>();
  TheSI = std::make_unique<StandardInstrumentations>(*TheContext,
                                                     /*DebugLogging*/ true);
 TheSI->registerCallbacks(*ThePIC,TheMAM.get());

 TheFPM->addPass(InstCombinePass());
 TheFPM->addPass(ReassociatePass());
 TheFPM->addPass(GVNPass());
 TheFPM->addPass(SimplifyCFGPass());

 PassBuilder PB;
 PB.registerModuleAnalyses(*TheMAM);
 PB.registerFunctionAnalyses(*TheFAM);
 PB.crossRegisterProxies(*TheLAM,*TheFAM,*TheCGAM,*TheMAM);



}

static void InitializedModule() {
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("my cool jit",*TheContext);

    Builder = std::make_unique<IRBuilder<>>(*TheContext);
}



static void HandleDefinition() {
    if(auto FnAST = ParseDefinition()) {
        if(auto *FnIR = FnAST->codegen()){
            fprintf(stderr,"Read function definition:");
            FnIR->print(errs());
            fprintf(stderr,"\n");
            ExitOnErr(TheJIT->addModule(ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
            InitializeModuleAndManagers();
        }
    } else {
        getNextToken();
    }
}


static void HandleExern() {
    if(auto ProtoAST = ParseExtern()){
        if(auto *FnIR = ProtoAST->codegen()){
            fprintf(stderr,"Read extern: ");
            FnIR->print(errs());
            fprintf(stderr,"\n");
        }
    } else {
        getNextToken();
    }
}

static void HandleTopLevelExpression() {
    if(auto FnAST = ParseTopLevelExpr()) {
        if(auto *FnIR = FnAST->codegen()) {
            fprintf(stderr,"read top-level expression: ");
            FnIR->print(errs());
            fprintf(stderr,"\n");

            FnIR->eraseFromParent();
        }
    } else {
        getNextToken();
    }
}


/// top ::= definition | external | expression | ';'
static void MainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons.
      getNextToken();
      break;
    case tok_def:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}








