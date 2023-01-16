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

#include "ast_node_literal.hpp"
#include "bamql-compiler.hpp"

namespace bamql {

enum RawFunctionArg { READ, HEADER, ERROR, USER };

class FunctionArg {
public:
  virtual void nextArg(ParseState &state,
                       size_t &pos,
                       std::vector<std::shared_ptr<AstNode>> &args) const = 0;
};

class UserArg final : public FunctionArg {
public:
  UserArg(ExprType type_);
  void nextArg(ParseState &state,
               size_t &pos,
               std::vector<std::shared_ptr<AstNode>> &args) const;

private:
  ExprType type;
};

class AuxArg final : public FunctionArg {
public:
  AuxArg() {}
  void nextArg(ParseState &state,
               size_t &pos,
               std::vector<std::shared_ptr<AstNode>> &args) const;
};

class NucleotideArg final : public FunctionArg {
public:
  NucleotideArg() {}
  void nextArg(ParseState &state,
               size_t &pos,
               std::vector<std::shared_ptr<AstNode>> &args) const;
};

template <typename T, typename F, F LF, ExprType ET>
class StaticArg final : public FunctionArg {
public:
  StaticArg(T value_) : value(value_) {}
  virtual void nextArg(ParseState &state,
                       size_t &pos,
                       std::vector<std::shared_ptr<AstNode>> &args) const {
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
               const std::vector<RawFunctionArg> &rawArguments_,
               ParseState &state);
  virtual llvm::Value *generateCall(GenerateState &state,
                                    llvm::FunctionType *functionType,
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
  const std::vector<RawFunctionArg> &rawArguments;
  const std::string name;
};

class BoolFunctionNode final : public FunctionNode {
public:
  BoolFunctionNode(const std::string &name_,
                   const std::vector<std::shared_ptr<AstNode>> &&arguments_,
                   const std::vector<RawFunctionArg> &rawArguments_,
                   ParseState &state);
  llvm::Value *generateCall(GenerateState &state,
                            llvm::FunctionType *functionType,
                            llvm::Function *func,
                            std::vector<llvm::Value *> &args,
                            llvm::Value *error_fun,
                            llvm::Value *error_ctx);
  ExprType type();
};
class ConstIntFunctionNode final : public FunctionNode {
public:
  ConstIntFunctionNode(const std::string &name_,
                       const std::vector<std::shared_ptr<AstNode>> &&arguments_,
                       const std::vector<RawFunctionArg> &rawArguments_,
                       ParseState &state);
  llvm::Value *generateCall(GenerateState &state,
                            llvm::FunctionType *functionType,
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
                    const std::vector<RawFunctionArg> &rawArguments_,
                    ParseState &state,
                    const std::string &error_message_);

  virtual void generateRead(GenerateState &state,
                            llvm::FunctionType *functionType,
                            llvm::Value *function,
                            std::vector<llvm::Value *> &args,
                            llvm::Value *&out_success,
                            llvm::Value *&out_result) = 0;
  llvm::Value *generateCall(GenerateState &state,
                            llvm::FunctionType *functionType,
                            llvm::Function *func,
                            std::vector<llvm::Value *> &args,
                            llvm::Value *error_fun,
                            llvm::Value *error_ctx);

private:
  std::string error_message;
};

class DblFunctionNode final : public ErrorFunctionNode {
public:
  DblFunctionNode(const std::string &name_,
                  const std::vector<std::shared_ptr<AstNode>> &&arguments_,
                  const std::vector<RawFunctionArg> &rawArguments_,
                  ParseState &state,
                  const std::string &error_message);
  void generateRead(GenerateState &state,
                    llvm::FunctionType *functionType,
                    llvm::Value *function,
                    std::vector<llvm::Value *> &args,
                    llvm::Value *&out_success,
                    llvm::Value *&out_result);
  ExprType type();
};
class IntFunctionNode final : public ErrorFunctionNode {
public:
  IntFunctionNode(const std::string &name_,
                  const std::vector<std::shared_ptr<AstNode>> &&arguments_,
                  const std::vector<RawFunctionArg> &rawArguments_,
                  ParseState &state,
                  const std::string &error_message);
  void generateRead(GenerateState &state,
                    llvm::FunctionType *functionType,
                    llvm::Value *function,
                    std::vector<llvm::Value *> &args,
                    llvm::Value *&out_success,
                    llvm::Value *&out_result);
  ExprType type();
};

class StrFunctionNode final : public ErrorFunctionNode {
public:
  StrFunctionNode(const std::string &name_,
                  const std::vector<std::shared_ptr<AstNode>> &&arguments_,
                  const std::vector<RawFunctionArg> &rawArguments_,

                  ParseState &state,
                  const std::string &error_message);
  void generateRead(GenerateState &state,
                    llvm::FunctionType *functionType,
                    llvm::Value *function,
                    std::vector<llvm::Value *> &args,
                    llvm::Value *&out_success,
                    llvm::Value *&out_result);
  ExprType type();
};

template <typename T, typename... Config>
Predicate parseFunction(
    const std::string &name,
    const std::vector<RawFunctionArg> rawArguments,
    const std::vector<std::reference_wrapper<const FunctionArg>> args,
    Config... extra_config) {
  return [=](ParseState &state) {
    std::vector<std::shared_ptr<AstNode>> arguments;
    size_t pos = 0;
    for (auto &arg : args) {
      arg.get().nextArg(state, pos, arguments);
    }
    if (pos > 0) {
      state.parseCharInSpace(')');
    }

    return std::make_shared<T>(name, std::move(arguments), rawArguments, state,
                               extra_config...);
  };
}
} // namespace bamql
