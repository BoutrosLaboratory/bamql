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

class LoopVar;
class LoopNode final : public AstNode {
  friend class LoopVar;

public:
  LoopNode(ParseState &state,
           const std::string &var_name,
           bool all_,
           std::vector<std::shared_ptr<AstNode>> &&values_);
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
} // namespace bamql
