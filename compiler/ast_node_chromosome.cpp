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

#include "ast_node_chromosome.hpp"
#include "bamql-compiler.hpp"
#include <htslib/sam.h>
#include <set>

static bamql::RegularExpression chrStrToRegex(const std::string &str) {
  // If we are dealing with a chromosome that goes by many names, match all of
  // them.
  for (const auto &set : bamql::equivalence_sets) {
    for (const auto &equiv : set) {
      if (equiv.length() == str.length() &&
          std::equal(equiv.begin(), equiv.end(), str.begin(),
                     [](unsigned char a, unsigned char b) {
                       return std::tolower(a) == std::tolower(b);
                     })) {
        return bamql::setToRegEx("^(chr)?", set, "$");
      }
    }
  }
  // otherwise, just match the provided chromosome.
  return bamql::globToRegEx("^(chr)?", str, "$");
}

namespace bamql {
// These must be lower case
const std::set<std::set<std::string>> equivalence_sets = {
  { "23", "x" }, { "24", "y" }, { "25", "m", "mt" }
};

CheckChromosomeNode::CheckChromosomeNode(RegularExpression &&name_,
                                         bool mate_,
                                         ParseState &state)
    : DebuggableNode(state), mate(mate_), name(std::move(name_)) {}
CheckChromosomeNode::CheckChromosomeNode(const std::string &str,
                                         bool mate,
                                         ParseState &state)
    : CheckChromosomeNode(chrStrToRegex(str), mate, state) {}

llvm::Value *CheckChromosomeNode::generate(GenerateState &state,
                                           llvm::Value *read,
                                           llvm::Value *header,
                                           llvm::Value *error_fn,
                                           llvm::Value *error_ctx) {
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
llvm::Value *CheckChromosomeNode::generateIndex(GenerateState &state,
                                                llvm::Value *chromosome,
                                                llvm::Value *header,
                                                llvm::Value *error_fn,
                                                llvm::Value *error_ctx) {
  if (mate) {
    return llvm::ConstantInt::getTrue(state.module()->getContext());
  }
  auto function = state.module()->getFunction("bamql_check_chromosome_id");
  llvm::Value *args[] = { header, chromosome, name(state) };
  return state->CreateCall(function, args);
}

bool CheckChromosomeNode::usesIndex() { return !mate; }

ExprType CheckChromosomeNode::type() { return BOOL; }

std::shared_ptr<AstNode> CheckChromosomeNode::parse(ParseState &state,
                                                    bool mate) {
  state.parseCharInSpace('(');

  auto str = state.parseStr(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_*?.");
  if (str.compare(0, 3, "chr") == 0) {
    throw ParseError(state.where(),
                     "Chromosome names must not start with `chr'.");
  }

  state.parseCharInSpace(')');
  return std::make_shared<CheckChromosomeNode>(str, mate, state);
}
}
