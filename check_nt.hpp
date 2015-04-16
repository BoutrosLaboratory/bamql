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
#include "boolean_constant.hpp"

namespace bamql {
/**
 * A predicate that matches a particular nucleotide.
 */
template <BoolConstant EXACT> class NucleotideNode : public DebuggableNode {
public:
  NucleotideNode(int32_t position_, unsigned char nt_, ParseState &state)
      : DebuggableNode(state), position(position_), nt(nt_) {}
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = state.module()->getFunction("check_nt");
    return state->CreateCall4(
        function,
        read,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm::getGlobalContext()),
                               position),
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvm::getGlobalContext()),
                               nt),
        EXACT(llvm::getGlobalContext()));
  }

  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    state.parseCharInSpace('(');
    auto position = state.parseInt();
    state.parseCharInSpace(',');
    auto nt = state.parseNucleotide();
    state.parseCharInSpace(')');
    return std::make_shared<NucleotideNode<EXACT>>(position, nt, state);
  }

private:
  int32_t position;
  unsigned char nt;
};
}
