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

#include "bamql-compiler.hpp"

namespace bamql {

/**
 * The sets of chromosome names that are considered synonymous because we can't
 * agree on a standard set of names as a society.
 */
extern const std::set<std::set<std::string>> equivalence_sets;
/**
 * A predicate that checks of the chromosome name.
 */
class CheckChromosomeNode : public DebuggableNode {
public:
  CheckChromosomeNode(RegularExpression &&name_, bool mate_, ParseState &state);

  CheckChromosomeNode(const std::string &str, bool mate, ParseState &state);

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

  static std::shared_ptr<AstNode> parse(ParseState &state,
                                        bool mate) throw(ParseError);

private:
  bool mate;
  RegularExpression name;
};
}
