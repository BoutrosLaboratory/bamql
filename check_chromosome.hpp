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
#include "bamql.hpp"

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
  CheckChromosomeNode(std::string name_, ParseState &state)
      : DebuggableNode(state), name(name_) {}
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = state.module()->getFunction("check_chromosome");
    return state->CreateCall4(
        function,
        read,
        header,
        state.createString(name),
        mate ? llvm::ConstantInt::getTrue(llvm::getGlobalContext())
             : llvm::ConstantInt::getFalse(llvm::getGlobalContext()));
  }
  virtual llvm::Value *generateIndex(GenerateState &state,
                                     llvm::Value *chromosome,
                                     llvm::Value *header) {
    if (mate) {
      return llvm::ConstantInt::getTrue(llvm::getGlobalContext());
    }
    auto function = state.module()->getFunction("check_chromosome_id");
    return state->CreateCall3(
        function, chromosome, header, state.createString(name));
  }

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
        for (auto i = 0; i < equiv->length() && same; i++) {
          same = tolower(str[i]) == (*equiv)[i];
        }
        if (!same) {
          continue;
        }
        std::shared_ptr<AstNode> node;
        for (auto equiv_str = set->begin(); equiv_str != set->end();
             equiv_str++) {
          if (node) {
            node = std::make_shared<OrNode>(
                std::make_shared<CheckChromosomeNode<mate>>(*equiv_str, state),
                node);
          } else {
            node =
                std::make_shared<CheckChromosomeNode<mate>>(*equiv_str, state);
          }
        }
        return node;
      }
    }
    // otherwise, just match the provided chromosome.
    return std::make_shared<CheckChromosomeNode<mate>>(str, state);
  }

private:
  std::string name;
};
}
