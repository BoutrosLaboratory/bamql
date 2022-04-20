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

#include "bamql-compiler.hpp"

namespace bamql {

llvm::Value *make_bool(llvm::LLVMContext &context, bool value);

llvm::Value *make_char(llvm::LLVMContext &context, char value);

llvm::Value *make_int(llvm::LLVMContext &context, int value);

llvm::Value *make_dbl(llvm::LLVMContext &context, double value);

template <typename T, typename F, F LF, ExprType ET>
class LiteralNode final : public AstNode {
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
} // namespace bamql
