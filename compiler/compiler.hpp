/*
 * Copyright 2015 Paul Boutros. For details, see COPYING. Our lawyer cats sez:
 *
 * OICR makes no representations whatsoever as to the SOFTWARE contained
 * herein.  It is experimental in nature and is provided WITHOUT WARRANTY OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE OR ANY OTHER WARRANTY,
 * EXPRESS OR IMPLIED. OICR MAKES NO REPRESENTATION OR WARRANTY THAT THE USE OF
 * THIS SOFTWARE WILL NOT INFRINGE ANY PATENT OR OTHER PROPRIETARY RIGHT.  By
 * downloading this SOFTWARE, your Institution hereby indemnifies OICR against
 * any loss, claim, damage or liability, of whatsoever kind or nature, which
 * may arise from your Institution's respective use, handling or storage of the
 * SOFTWARE. If publications result from research using this SOFTWARE, we ask
 * that the Ontario Institute for Cancer Research be acknowledged and/or
 * credit be given to OICR scientists, as scientifically appropriate.
 */
#pragma once

#include <cstdlib>
#include <iostream>
#include "bamql-compiler.hpp"
#define type_check(EXPRESSION, TYPE)                                           \
  do {                                                                         \
    if ((EXPRESSION)->type() != (TYPE)) {                                      \
      std::cerr << __FILE__ ":" << __LINE__                                    \
                << ": Expression " #EXPRESSION " is not of type " #TYPE "\n";  \
      abort();                                                                 \
    }                                                                          \
  } while (0)

namespace bamql {

typedef llvm::Value *(llvm::IRBuilder<>::*CreateICmp)(llvm::Value *lhs,
                                                      llvm::Value *rhs,
                                                      const llvm::Twine &name);
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 7
typedef llvm::Value *(llvm::IRBuilder<>::*CreateFCmp)(llvm::Value *lhs,
                                                      llvm::Value *rhs,
                                                      const llvm::Twine &name);
#else
typedef llvm::Value *(llvm::IRBuilder<>::*CreateFCmp)(llvm::Value *lhs,
                                                      llvm::Value *rhs,
                                                      const llvm::Twine &name,
                                                      llvm::MDNode *fpmathtag);
#endif
/**
 * A syntax node for ternary if.
 */
class ConditionalNode : public AstNode {
public:
  ConditionalNode(const std::shared_ptr<AstNode> &condition,
                  const std::shared_ptr<AstNode> &then_part,
                  const std::shared_ptr<AstNode> &else_part);
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::Value *error_fn,
                                llvm::Value *error_ctx);
  virtual llvm::Value *generateIndex(GenerateState &state,
                                     llvm::Value *tid,
                                     llvm::Value *header,
                                     llvm::Value *error_fn,
                                     llvm::Value *error_ctx);
  bool usesIndex();
  ExprType type();
  void writeDebug(GenerateState &state);

private:
  std::shared_ptr<AstNode> condition;
  std::shared_ptr<AstNode> then_part;
  std::shared_ptr<AstNode> else_part;
};

class CompareFPNode : public DebuggableNode {
public:
  CompareFPNode(CreateFCmp comparator,
                std::shared_ptr<AstNode> &left,
                std::shared_ptr<AstNode> &right,
                ParseState &state);
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);
  ExprType type();

private:
  CreateFCmp comparator;
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};

class CompareIntNode : public DebuggableNode {
public:
  CompareIntNode(CreateICmp comparator,
                 std::shared_ptr<AstNode> &left,
                 std::shared_ptr<AstNode> &right,
                 ParseState &state);
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);
  ExprType type();

private:
  CreateICmp comparator;
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};

class CompareStrNode : public DebuggableNode {
public:
  CompareStrNode(CreateICmp comparator,
                 std::shared_ptr<AstNode> &left,
                 std::shared_ptr<AstNode> &right,
                 ParseState &state);
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);
  ExprType type();

private:
  CreateICmp comparator;
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};

class RegexNode : public DebuggableNode {
public:
  RegexNode(std::shared_ptr<AstNode> &operand,
            RegularExpression &&pattern,
            ParseState &state);
  llvm::Value *generateGeneric(GenerateMember member,
                               GenerateState &state,
                               llvm::Value *read,
                               llvm::Value *header,
                               llvm::Value *error_fn,
                               llvm::Value *error_ctx);
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *read,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx);
  bool usesIndex();
  ExprType type();

private:
  std::shared_ptr<AstNode> operand;
  RegularExpression pattern;
};

llvm::Value *make_bool(llvm::LLVMContext &context, bool value);

llvm::Value *make_char(llvm::LLVMContext &context, char value);

llvm::Value *make_int(llvm::LLVMContext &context, int value);

llvm::Value *make_dbl(llvm::LLVMContext &context, double value);

template <typename T, typename F, F LF, ExprType ET>
class LiteralNode : public AstNode {
public:
  LiteralNode(T value_) : value(value_) {}
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx) {
    return LF(state.module()->getContext(), value);
  }
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *tid,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx) {
    return LF(state.module()->getContext(), value);
  }
  ExprType type() { return ET; }
  void writeDebug(GenerateState &state) {}

private:
  T value;
};

typedef LiteralNode<bool, decltype(&make_bool), &make_bool, BOOL> BoolConst;
typedef LiteralNode<char, decltype(&make_char), &make_char, INT> CharConst;
typedef LiteralNode<double, decltype(&make_dbl), &make_dbl, FP> DblConst;
typedef LiteralNode<int, decltype(&make_int), &make_int, INT> IntConst;
class FunctionArg {
public:
  virtual void nextArg(ParseState &state,
                       size_t &pos,
                       std::vector<std::shared_ptr<AstNode>> &args) const
      throw(ParseError) = 0;
};

class UserArg : public FunctionArg {
public:
  UserArg(ExprType type_);
  void nextArg(ParseState &state,
               size_t &pos,
               std::vector<std::shared_ptr<AstNode>> &args) const
      throw(ParseError);

private:
  ExprType type;
};

class AuxArg : public FunctionArg {
public:
  AuxArg() {}
  void nextArg(ParseState &state,
               size_t &pos,
               std::vector<std::shared_ptr<AstNode>> &args) const
      throw(ParseError);
};
class NucleotideArg : public FunctionArg {
public:
  NucleotideArg() {}
  void nextArg(ParseState &state,
               size_t &pos,
               std::vector<std::shared_ptr<AstNode>> &args) const
      throw(ParseError);
};

template <typename T, typename F, F LF, ExprType ET>
class StaticArg : public FunctionArg {
public:
  StaticArg(T value_) : value(value_) {}
  virtual void nextArg(ParseState &state,
                       size_t &pos,
                       std::vector<std::shared_ptr<AstNode>> &args) const
      throw(ParseError) {
    args.push_back(std::make_shared<LiteralNode<T, F, LF, ET>>(value));
  }

private:
  T value;
};

typedef StaticArg<bool, decltype(&make_bool), &make_bool, BOOL> BoolArg;
typedef StaticArg<char, decltype(&make_char), &make_char, INT> CharArg;
typedef StaticArg<int, decltype(&make_int), &make_int, INT> IntArg;
typedef StaticArg<double, decltype(&make_dbl), &make_dbl, FP> DblArg;

/**
 * Call a runtime library function with parameters
 */
class FunctionNode : public DebuggableNode {
public:
  FunctionNode(const std::string &name_,
               const std::vector<std::shared_ptr<AstNode>> &&arguments_,
               ParseState &state);
  virtual llvm::Value *generateCall(GenerateState &state,
                                    llvm::Function *func,
                                    std::vector<llvm::Value *> &args,
                                    llvm::Value *error_fun,
                                    llvm::Value *error_ctx) = 0;
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);

private:
  const std::vector<std::shared_ptr<AstNode>> arguments;
  const std::string name;
};

class BoolFunctionNode : public FunctionNode {
public:
  BoolFunctionNode(const std::string &name_,
                   const std::vector<std::shared_ptr<AstNode>> &&arguments_,
                   ParseState &state);
  llvm::Value *generateCall(GenerateState &state,
                            llvm::Function *func,
                            std::vector<llvm::Value *> &args,
                            llvm::Value *error_fun,
                            llvm::Value *error_ctx);
  ExprType type();
};

class ErrorFunctionNode : public FunctionNode {
public:
  ErrorFunctionNode(const std::string &name_,
                    const std::vector<std::shared_ptr<AstNode>> &&arguments_,
                    ParseState &state,
                    const std::string &error_message_);

  virtual void generateRead(GenerateState &state,
                            llvm::Value *function,
                            std::vector<llvm::Value *> &args,
                            llvm::Value *&out_success,
                            llvm::Value *&out_result) = 0;
  llvm::Value *generateCall(GenerateState &state,
                            llvm::Function *func,
                            std::vector<llvm::Value *> &args,
                            llvm::Value *error_fun,
                            llvm::Value *error_ctx);

private:
  std::string error_message;
};

class DblFunctionNode : public ErrorFunctionNode {
public:
  DblFunctionNode(const std::string &name_,
                  const std::vector<std::shared_ptr<AstNode>> &&arguments_,
                  ParseState &state,
                  const std::string &error_message);
  void generateRead(GenerateState &state,
                    llvm::Value *function,
                    std::vector<llvm::Value *> &args,
                    llvm::Value *&out_success,
                    llvm::Value *&out_result);
  ExprType type();
};
class IntFunctionNode : public ErrorFunctionNode {
public:
  IntFunctionNode(const std::string &name_,
                  const std::vector<std::shared_ptr<AstNode>> &&arguments_,
                  ParseState &state,
                  const std::string &error_message);
  void generateRead(GenerateState &state,
                    llvm::Value *function,
                    std::vector<llvm::Value *> &args,
                    llvm::Value *&out_success,
                    llvm::Value *&out_result);
  ExprType type();
};

class StrFunctionNode : public ErrorFunctionNode {
public:
  StrFunctionNode(const std::string &name_,
                  const std::vector<std::shared_ptr<AstNode>> &&arguments_,
                  ParseState &state,
                  const std::string &error_message);
  void generateRead(GenerateState &state,
                    llvm::Value *function,
                    std::vector<llvm::Value *> &args,
                    llvm::Value *&out_success,
                    llvm::Value *&out_result);
  ExprType type();
};

template <typename T, typename... Config>
Predicate parseFunction(
    const std::string &name,
    const std::vector<std::reference_wrapper<const FunctionArg>> args,
    Config... extra_config) throw(ParseError) {
  return [=](ParseState &state) {
    std::vector<std::shared_ptr<AstNode>> arguments;
    size_t pos = 0;
    for (auto arg = args.begin(); arg != args.end(); arg++) {
      arg->get().nextArg(state, pos, arguments);
    }
    if (pos > 0) {
      state.parseCharInSpace(')');
    }

    return std::make_shared<T>(
        name, std::move(arguments), state, extra_config...);
  };
}

class LoopVar;
class LoopNode : public AstNode {
  friend class LoopVar;

public:
  LoopNode(ParseState &state,
           const std::string &var_name,
           bool all_,
           std::vector<std::shared_ptr<AstNode>> &&values_) throw(ParseError);
  LoopNode(bool all,
           std::shared_ptr<AstNode> &body,
           std::vector<std::shared_ptr<AstNode>> &values);
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *chromosome,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx);
  bool usesIndex();
  ExprType type();
  void writeDebug(GenerateState &state);

private:
  bool all;
  std::shared_ptr<AstNode> body;
  std::vector<std::shared_ptr<AstNode>> values;
  std::shared_ptr<LoopVar> var;
};

std::shared_ptr<AstNode> parseBED(ParseState &state) throw(ParseError);
std::shared_ptr<AstNode> parseBinding(ParseState &state) throw(ParseError);
std::shared_ptr<AstNode> parseMatchBinding(ParseState &state) throw(ParseError);
std::shared_ptr<AstNode> parseMax(ParseState &state) throw(ParseError);
std::shared_ptr<AstNode> parseMin(ParseState &state) throw(ParseError);
}
