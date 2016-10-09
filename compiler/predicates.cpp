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
#include <iostream>
#include <htslib/sam.h>
#include "bamql-compiler.hpp"
#include "compiler.hpp"
#include "check_chromosome.hpp"
#include "check_flag.hpp"
#include "constant.hpp"

// Please keep the predicates in alphabetical order.
namespace bamql {

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
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::Value *error_fn,
                                llvm::Value *error_ctx) {
    auto function = state.module()->getFunction("bamql_check_mapping_quality");
    llvm::Value *args[] = { read,
                            llvm::ConstantInt::get(
                                llvm::Type::getInt8Ty(
                                    state.module()->getContext()),
                                -10 * log(probability)) };
    return state->CreateCall(function, args);
  }

  ExprType type() { return BOOL; }
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
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::Value *error_fn,
                                llvm::Value *error_ctx) {
    auto function = state.module()->getFunction("bamql_randomly");
    llvm::Value *args[] = { llvm::ConstantFP::get(
        llvm::Type::getDoubleTy(state.module()->getContext()), probability) };
    return state->CreateCall(function, args);
  }

  ExprType type() { return BOOL; }
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

static const UserArg bool_arg(BOOL);
static const UserArg fp_arg(FP);
static const UserArg int_arg(INT);
static const AuxArg aux_arg;
static const NucleotideArg nucleotide_arg;
static const BoolArg true_arg(true);
static const BoolArg false_arg(false);
static const CharArg char_r('R');
static const CharArg char_g('G');
static const IntArg int_max_arg(INT32_MAX);
static const IntArg int_zero_arg(0);

/**
 * All the predicates known to the system.
 */
PredicateMap getDefaultPredicates() {

  return {
    // Auxiliary data
    { "read_group",
      parseFunction<StrFunctionNode, const std::string &>(
          "bamql_aux_str", { char_r, char_g }, "Read group not available.") },
    { "aux_str",
      parseFunction<StrFunctionNode, const std::string &>(
          "bamql_aux_str", { aux_arg }, "Auxiliary string not available.") },
    { "aux_int",
      parseFunction<IntFunctionNode, const std::string &>(
          "bamql_aux_int", { aux_arg }, "Auxiliary integer not available.") },
    { "aux_dbl",
      parseFunction<DblFunctionNode, const std::string &>(
          "bamql_aux_fp", { aux_arg }, "Auxiliary double not available.") },

    // Chromosome information
    { "chr", CheckChromosomeNode<false>::parse },
    { "mate_chr", CheckChromosomeNode<true>::parse },
    { "chr_name",
      parseFunction<StrFunctionNode, const std::string &>(
          "bamql_chr", { false_arg }, "Read not mapped.") },
    { "mate_chr_name",
      parseFunction<StrFunctionNode, const std::string &>(
          "bamql_chr", { true_arg }, "Read's mate not mapped.") },

    // Flags
    { "duplicate?", CheckFlag<BAM_FDUP>::parse },
    { "failed_qc?", CheckFlag<BAM_FQCFAIL>::parse },
    { "mapped_to_reverse?", CheckFlag<BAM_FREVERSE>::parse },
    { "mate_mapped_to_reverse?",
      CheckFlag<BAM_FPAIRED | BAM_FMREVERSE>::parse },
    { "mate_unmapped?", CheckFlag<BAM_FPAIRED | BAM_FMUNMAP>::parse },
    { "paired?", CheckFlag<BAM_FPAIRED>::parse },
    { "proper_pair?", CheckFlag<BAM_FPAIRED | BAM_FPROPER_PAIR>::parse },
    { "raw_flag", parseFunction<BoolFunctionNode>("check_flag", { int_arg }) },
    { "read1?", CheckFlag<BAM_FPAIRED | BAM_FREAD1>::parse },
    { "read2?", CheckFlag<BAM_FPAIRED | BAM_FREAD2>::parse },
    { "secondary?", CheckFlag<BAM_FSECONDARY>::parse },
    { "supplementary?", CheckFlag<BAM_FSUPPLEMENTARY>::parse },
    { "unmapped?", CheckFlag<BAM_FUNMAP>::parse },

    // Constants
    { "false", FalseNode::parse },
    { "true", TrueNode::parse },

    // Position
    { "after",
      parseFunction<BoolFunctionNode>("bamql_check_position",
                                      { int_arg, int_max_arg }) },
    { "before",
      parseFunction<BoolFunctionNode>("bamql_check_position",
                                      { int_zero_arg, int_arg }) },
    { "position",
      parseFunction<BoolFunctionNode>("bamql_check_position",
                                      { int_arg, int_arg }) },

    { "begin",
      parseFunction<IntFunctionNode, const std::string &>(
          "bamql_position_begin", {}, "Read is not mapped.") },
    { "end",
      parseFunction<IntFunctionNode, const std::string &>(
          "bamql_position_end", {}, "Read is not mapped.") },

    // Miscellaneous
    { "mapping_quality", MappingQualityNode::parse },
    { "header",
      parseFunction<StrFunctionNode, const std::string &>(
          "bamql_header", {}, "Header not available.") },
    { "nt",
      parseFunction<BoolFunctionNode>("bamql_check_nt",
                                      { int_arg, nucleotide_arg, false_arg }) },
    { "nt_exact",
      parseFunction<BoolFunctionNode>("bamql_check_nt",
                                      { int_arg, nucleotide_arg, true_arg }) },
    { "split_pair?",
      parseFunction<BoolFunctionNode>("bamql_check_split_pair", {}) },
    { "random", RandomlyNode::parse },
  };
}
}
