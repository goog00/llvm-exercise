#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>


using namespace llvm;

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

  // skip any whitespace
  while (isspace(LastChar)) {
    LastChar = getchar();
  }

  if (isalpha(LastChar)) {
    // identifier: [a-zA-Z][a-zA-Z0-9]*
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;

    if (IdentifierStr == "extern") {
      return tok_extern;
    }

    return tok_identifier;
  }

  // Number: [0-9.]+
  if (isdigit(LastChar) || LastChar == '.') {
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), 0);
    return tok_number;
  }

  if (LastChar == '#') {
    do {
      LastChar = getchar();
    } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = getchar();

  return ThisChar;
}

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//

// ExprAST - Base class for all expression nodes
class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual Value *codegen() = 0;
};

// NumberExprAST - Expression class for numeric literals like "1.0"
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
  Value *codegen() override;
};

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
  // 使用std::move将Args的值移动到类的成员变量Args中，是为了高效地移动数据，而不是进行拷贝
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

    Function *codegen();

  const std::string &getName() const { return Name; }
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

auto LHS = std::make_unique<VariableExprAST>("x");
auto RHS = std::make_unique<VariableExprAST>("y");
auto Result =
    std::make_unique<BinaryExprAST>('+', std::move(LHS), std::move(RHS));

static int CurTok;
static int getNextToken() { return CurTok = gettok(); }

std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error:%s\n", Str);
  return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}

static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken();
  return std::move(Result);
}

static std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // eat(.
  auto V = ParseExpression();
  if (!V) {
    return nullptr;
  }
  if (CurTok != ')')
    return LogError("exprected ')");

  getNextToken(); // eat ).
  return V;
}

/// identifierexpr
/// ::= identifier
/// ::= identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;

  getNextToken(); // eat identifier

  if (CurTok != '(') // Simple variable ref
    return std::make_unique<VariableExprAST>(IdName);

  // call
  getNextToken();
  std::vector<std::unique_ptr<ExprAST>> Args;

  if (CurTok != ')') {
    while (true) {
      if (auto Arg = ParseExpression()) {
        Args.push_back(std::move(Arg));
      } else {
        return nullptr;
      }

      if (CurTok == ')') {
        break;
      }

      if (CurTok != ',')
        return LogError("expected ')' or ',' in argument list");

      getNextToken();
    }
  }

  // eat the ')'
  getNextToken();
  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

/// primary
/// ::= identifierexpr
/// ::= numberexpr
/// ::=parenexpr

static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  default:
    return LogError("unknown token when expection an expression");
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  }
}

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.

static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence -Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}

static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS) {
    return nullptr;
  }
  return ParseBinOpRHS(0, std::move(LHS));
}

/// @brief  binoprhs
///              ::=('+' primary)*
/// @param ExprPrec
/// @param LHS
/// @return
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = GetTokPrecedence();

    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    getNextToken();

    auto RHS = ParsePrimary();
    if (!RHS) {
      return nullptr;
    }

    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    // Merge LHS/RHS.
    LHS =
        std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
  }

 
}

/// prototype
///   ::= id '(' id* ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
  if (CurTok != tok_identifier)
    return LogErrorP("Expected function name in prototype");

  std::string FnName = IdentifierStr;
  getNextToken();

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  // success.
  getNextToken(); // eat ')'.

  return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}


static std::unique_ptr<FunctionAST> ParseDefinition(){
    getNextToken();// eat def
    auto Proto = ParsePrototype();
    if(!Proto) return nullptr;

    if(auto E = ParseExpression())
       return std::make_unique<FunctionAST>(std::move(Proto),std::move(E));

    return nullptr;   
}

static std::unique_ptr<PrototypeAST>  ParseExtern() {
    getNextToken();// eat extern.
    return ParsePrototype();
}

static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if(auto E = ParseExpression()) {
        auto Proto = std::make_unique<PrototypeAST>("",std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto),std::move(E));
    }
    return nullptr;
}


//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//

static void HandleDefinition() {
  if (ParseDefinition()) {
    fprintf(stderr, "Parsed a function definition.\n");
  } else {
    // Skip token for error recovery.
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


//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//
static std::unique_ptr<LLVMContext> TheContext;
static std::unique_ptr<IRBuilder<>> Builder;
static std::unique_ptr<Module> TheModule;
static std::map<std::string,Value *> NamedValues;

Value *LogErrorV(const char *Str){
    LogError(Str);
    return nullptr;
}


Value *NumberExprAST::codegen(){
    return ConstantFP::get(*TheContext,APFloat(Val));
}

Value *VariableExprAST::codegen(){

    Value *V = NamedValues[Name];
    if(!V)
       LogErrorV("Unknown variable name");

    return V;   
}


Value *BinaryExprAST::codegen() {
    Value *L = LHS->codegen();
    Value *R = RHS->codegen();

    if(!L || !R){
        return nullptr;
    }

    switch (Op)
    {
    case '+':
        return Builder->CreateFAdd(L,R,"addtmp");
       
    case '-':
        return Builder->CreateFSub(L,R,"subtmp");
    case '*':
        return Builder->CreateFMul(L,R,"multmp");

    case '<':
        L = Builder->CreateFCmpULT(L,R,"cmtmp");
        return Builder->CreateUIToFP(L,Type::getDoubleTy(*TheContext),"booltmp");        
    default:
       return LogErrorV("invalid binary operator");
    }
}


Value *CallExprAST::codegen() {
    Function *CalleeF = TheModule->getFunction(Callee);
    if(!CalleeF)
      return LogErrorV("Unknown function referenced");

    if(CalleeF->arg_size() != Args.size()){
        return LogErrorV("Incorrect # arguments passed");
    }  

    std::vector<Value *> ArgsV;

    for(unsigned i = 0,e = Args.size(); i != e; ++i){
        ArgsV.push_back(Args[i]->codegen());
        if(!ArgsV.back()){
            return nullptr;
        }
    }

    return Builder->CreateCall(CalleeF,ArgsV,"calltmp");

}


Function *PrototypeAST::codegen() {
    std::vector<Type*> Doubles(Args.size(),Type::getDoubleTy(*TheContext));

    FunctionType *FT = FunctionType::get(Type::getDoubleTy(*TheContext),Doubles,false);

    Function *F = Function::Create(FT,Function::ExternalLinkage,Name,TheModule.get());

    unsigned Idx = 0;
    for(auto &Arg : F->args())
      Arg.setName(Args[Idx++]);

    return F;  


}


Function *FunctionAST::codegen(){

    Function *TheFunction = TheModule->getFunction(Proto->getName());

    if(!TheFunction){
        TheFunction = Proto->codegen();
    }

    if(!TheFunction){
        return nullptr;
    }

    if(!TheFunction->empty())
    return (Function*)LogErrorV("Function cannot be redefined.");



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
// Main driver code.
//===----------------------------------------------------------------------===//

int main() {
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40; // highest.

  // Prime the first token.
  fprintf(stderr, "ready> ");
  getNextToken();

  // Run the main "interpreter loop" now.
  MainLoop();

  return 0;
}
