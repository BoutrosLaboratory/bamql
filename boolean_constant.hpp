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
#include <htslib/sam.h>
#include "bamql.hpp"

namespace bamql {
typedef llvm::ConstantInt *(*BoolConstant)(llvm::LLVMContext &);

/**
 * A predicate that always returns a constant.
 */
template <BoolConstant CF> class ConstantNode : public AstNode {
public:
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header) {
    return CF(state.module()->getContext());
  }
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *tid,
                             llvm::Value *header) {
    return CF(state.module()->getContext());
  }
  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    static auto result = std::make_shared<ConstantNode<CF>>();
    return result;
  }
  void writeDebug(GenerateState &state) {}
};
}
