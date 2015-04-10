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

#include <cstdint>
#include <htslib/sam.h>
#include "bamql.hpp"
#include "boolean_constant.hpp"
#include "check_aux.hpp"
#include "check_chromosome.hpp"
#include "check_flag.hpp"
#include "check_nt.hpp"

// Please keep the predicates in alphabetical order.
namespace bamql {

/* This insanity is brought to you by samtools's sam_tview.c */
static bool readGroupChar(char input, bool not_first) {
  return input >= '!' && input <= '~' && (not_first || input != '=');
}

// These must be lower case
const std::set<std::set<std::string>> equivalence_sets = {
  { "23", "x" }, { "24", "y" }, { "25", "m", "mt" }
};

/**
 * A predicate that is true if the mapping quality is sufficiently good.
 */
class MappingQualityNode : public DebuggableNode {
public:
  MappingQualityNode(double probability_, ParseState &state)
      : DebuggableNode(state), probability(probability_) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::DIScope *debug_scope) {
    auto function = module->getFunction("check_mapping_quality");
    return builder.CreateCall2(
        function,
        read,
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvm::getGlobalContext()),
                               -10 * log(probability)));
  }

  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    state.parseCharInSpace('(');

    auto probability = state.parseDouble();
    if (probability <= 0 || probability >= 1) {
      throw ParseError(state.where(),
                       "The provided probability is not probable.");
    }

    state.parseCharInSpace(')');

    return std::make_shared<MappingQualityNode>(probability, state);
  }

private:
  double probability;
};

/**
 * A predicate that randomly is true.
 */
class RandomlyNode : public DebuggableNode {
public:
  RandomlyNode(double probability_, ParseState &state)
      : DebuggableNode(state), probability(probability_) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::DIScope *debug_scope) {
    auto function = module->getFunction("randomly");
    return builder.CreateCall(
        function,
        llvm::ConstantFP::get(llvm::Type::getDoubleTy(llvm::getGlobalContext()),
                              probability));
  }

  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    state.parseCharInSpace('(');

    auto probability = state.parseDouble();
    if (probability < 0 || probability > 1) {
      throw ParseError(state.where(),
                       "The provided probability is not probable.");
    }

    state.parseCharInSpace(')');

    return std::make_shared<RandomlyNode>(probability, state);
  }

private:
  double probability;
};

/**
 * A predicate that check mapped positions.
 */
class RawFlagNode : public DebuggableNode {
public:
  RawFlagNode(uint32_t raw_, ParseState &state)
      : DebuggableNode(state), raw(raw_) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::DIScope *debug_scope) {
    auto function = module->getFunction("check_flag");
    return builder.CreateCall2(
        function,
        read,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm::getGlobalContext()),
                               raw));
  }

  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    state.parseCharInSpace('(');
    auto raw = state.parseInt();
    state.parseCharInSpace(')');
    return std::make_shared<RawFlagNode>(raw, state);
  }

private:
  uint32_t raw;
};

/**
 * A predicate that check mapped positions.
 */
class PositionNode : public DebuggableNode {
public:
  PositionNode(int32_t start_, int32_t end_, ParseState &state)
      : DebuggableNode(state), start(start_), end(end_) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::DIScope *debug_scope) {
    auto function = module->getFunction("check_position");
    return builder.CreateCall3(
        function,
        read,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm::getGlobalContext()),
                               start),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm::getGlobalContext()),
                               end));
  }

  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    state.parseCharInSpace('(');
    auto start = state.parseInt();
    state.parseCharInSpace(',');
    auto end = state.parseInt();
    state.parseCharInSpace(')');
    return std::make_shared<PositionNode>(start, end, state);
  }
  static std::shared_ptr<AstNode> parseAfter(ParseState &state) throw(
      ParseError) {
    state.parseCharInSpace('(');
    auto pos = state.parseInt();
    state.parseCharInSpace(')');
    return std::make_shared<PositionNode>(pos, INT32_MAX, state);
  }
  static std::shared_ptr<AstNode> parseBefore(ParseState &state) throw(
      ParseError) {
    state.parseCharInSpace('(');
    auto pos = state.parseInt();
    state.parseCharInSpace(')');
    return std::make_shared<PositionNode>(0, pos, state);
  }

private:
  int32_t start;
  int32_t end;
};

class SplitPairNode : public DebuggableNode {
public:
  SplitPairNode(ParseState &state) : DebuggableNode(state) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::DIScope *debug_scope) {
    auto function = module->getFunction("check_split_pair");
    return builder.CreateCall2(function, header, read);
  }

  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    return std::make_shared<SplitPairNode>(state);
  }
};

/**
 * All the predicates known to the system.
 */
PredicateMap getDefaultPredicates() {

  return {
    // Auxiliary data
    { std::string("read_group"),
      CheckAuxStringNode<'R', 'G', readGroupChar>::parse },

    // Chromosome information
    { std::string("chr"), CheckChromosomeNode<false>::parse },
    { std::string("mate_chr"), CheckChromosomeNode<true>::parse },

    // Flags
    { std::string("duplicate?"), CheckFlag<BAM_FDUP>::parse },
    { std::string("failed_qc?"), CheckFlag<BAM_FQCFAIL>::parse },
    { std::string("mapped_to_reverse?"), CheckFlag<BAM_FREVERSE>::parse },
    { std::string("mate_mapped_to_reverse?"),
      CheckFlag<BAM_FPAIRED | BAM_FMREVERSE>::parse },
    { std::string("mate_unmapped?"),
      CheckFlag<BAM_FPAIRED | BAM_FMUNMAP>::parse },
    { std::string("paired?"), CheckFlag<BAM_FPAIRED>::parse },
    { std::string("proper_pair?"),
      CheckFlag<BAM_FPAIRED | BAM_FPROPER_PAIR>::parse },
    { std::string("raw_flag"), RawFlagNode::parse },
    { std::string("read1?"), CheckFlag<BAM_FPAIRED | BAM_FREAD1>::parse },
    { std::string("read2?"), CheckFlag<BAM_FPAIRED | BAM_FREAD2>::parse },
    { std::string("secondary?"), CheckFlag<BAM_FSECONDARY>::parse },
    { std::string("supplementary?"), CheckFlag<BAM_FSUPPLEMENTARY>::parse },
    { std::string("unmapped?"), CheckFlag<BAM_FUNMAP>::parse },

    // Constants
    { std::string("false"), ConstantNode<llvm::ConstantInt::getFalse>::parse },
    { std::string("true"), ConstantNode<llvm::ConstantInt::getTrue>::parse },

    // Position
    { std::string("after"), PositionNode::parseAfter },
    { std::string("before"), PositionNode::parseBefore },
    { std::string("position"), PositionNode::parse },

    // Miscellaneous
    { std::string("mapping_quality"), MappingQualityNode::parse },
    { std::string("nt"), NucleotideNode<llvm::ConstantInt::getFalse>::parse },
    { std::string("nt_exact"),
      NucleotideNode<llvm::ConstantInt::getTrue>::parse },
    { std::string("split_pair?"), SplitPairNode::parse },
    { std::string("random"), RandomlyNode::parse }
  };
}
}
