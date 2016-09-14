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

#include <set>
#include <htslib/sam.h>
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
template <bool mate> class CheckChromosomeNode : public DebuggableNode {
public:
  CheckChromosomeNode(RegularExpression &&name_, ParseState &state)
      : DebuggableNode(state), name(std::move(name_)) {}
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = state.module()->getFunction("bamql_check_chromosome");
    llvm::Value *args[] = {
      header,
      read,
      name(state),
      mate ? llvm::ConstantInt::getTrue(state.module()->getContext())
           : llvm::ConstantInt::getFalse(state.module()->getContext())
    };
    return state->CreateCall(function, args);
  }
  virtual llvm::Value *generateIndex(GenerateState &state,
                                     llvm::Value *chromosome,
                                     llvm::Value *header) {
    if (mate) {
      return llvm::ConstantInt::getTrue(state.module()->getContext());
    }
    auto function = state.module()->getFunction("bamql_check_chromosome_id");
    llvm::Value *args[] = { header, chromosome, name(state) };
    return state->CreateCall(function, args);
  }

  bool usesIndex() { return !mate; }

  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    state.parseCharInSpace('(');

    auto str = state.parseStr(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_*?.");
    if (str.compare(0, 3, "chr") == 0) {
      throw ParseError(state.where(),
                       "Chromosome names must not start with `chr'.");
    }

    state.parseCharInSpace(')');

    // If we are dealing with a chromosome that goes by many names, match all of
    // them.
    for (auto set = equivalence_sets.begin(); set != equivalence_sets.end();
         set++) {
      for (auto equiv = set->begin(); equiv != set->end(); equiv++) {
        bool same = equiv->length() == str.length();
        for (size_t i = 0; i < equiv->length() && same; i++) {
          same = tolower(str[i]) == (*equiv)[i];
        }
        if (!same) {
          continue;
        }
        return std::make_shared<CheckChromosomeNode<mate>>(
            setToRegEx("^(chr)?", *set, "$"), state);
      }
    }
    // otherwise, just match the provided chromosome.
    return std::make_shared<CheckChromosomeNode<mate>>(
        globToRegEx("^(chr)?", str, "$"), state);
  }

private:
  RegularExpression name;
};
}
