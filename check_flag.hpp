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

#include <htslib/sam.h>
#include "bamql.hpp"

namespace bamql {

/**
 * A predicate that checks of the read's flag.
 */
template <unsigned int F> class CheckFlag : public DebuggableNode {
public:
  CheckFlag(ParseState &state) : DebuggableNode(state) {}
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = state.module()->getFunction("bamql_check_flag");
    llvm::Value *args[] = {
      read,
      llvm::ConstantInt::get(
          llvm::Type::getInt16Ty(state.module()->getContext()), F)
    };
    return state->CreateCall(function, args);
  }

  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    static auto result = std::make_shared<CheckFlag<F>>(state);
    return result;
  }
};
}
