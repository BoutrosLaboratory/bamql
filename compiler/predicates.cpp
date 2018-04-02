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
#include "bamql-compiler.hpp"
#include "compiler.hpp"
#include "ast_node_chromosome.hpp"
#include "ast_node_contains.hpp"
#include "ast_node_function.hpp"

// Please keep the predicates in alphabetical order.
namespace bamql {

class ProbabilityArg : public FunctionArg {
public:
  void nextArg(ParseState &state,
               size_t &pos,
               std::vector<std::shared_ptr<AstNode>> &args) const
      throw(ParseError) {
    state.parseCharInSpace(pos == 0 ? '(' : ',');
    pos++;
    auto probability = state.parseDouble();
    if (probability <= 0 || probability >= 1) {
      throw ParseError(state.where(),
                       "The provided probability is not probable.");
    }
    args.push_back(makeAst(probability));
  }

protected:
  virtual std::shared_ptr<AstNode> makeAst(double probability) const = 0;
};

class FixedProbabilityArg final : public ProbabilityArg {
public:
  FixedProbabilityArg() {}

protected:
  std::shared_ptr<AstNode> makeAst(double probability) const {
    return std::make_shared<
        LiteralNode<double, decltype(&make_dbl), &make_dbl, FP>>(probability);
  }
};
class MappingQualityArg final : public ProbabilityArg {
public:
  MappingQualityArg() {}

protected:
  std::shared_ptr<AstNode> makeAst(double probability) const {
    return std::make_shared<
        LiteralNode<char, decltype(&make_char), &make_char, INT>>(
        -10 * log(probability));
  }
};

static std::shared_ptr<BoolConst> falseNode(new BoolConst(false));
static std::shared_ptr<BoolConst> trueNode(new BoolConst(true));

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
static const FixedProbabilityArg fixed_probability_arg;
static const MappingQualityArg mapping_quality_arg;

static std::shared_ptr<AstNode> parseFlag(ParseState &state,
                                          uint32_t flag) throw(ParseError) {
  std::vector<std::shared_ptr<AstNode>> args;
  auto haystack =
      std::static_pointer_cast<AstNode>(std::make_shared<ConstIntFunctionNode>(
          "bamql_flags", std::move(args), state));
  auto needle =
      std::static_pointer_cast<AstNode>(std::make_shared<IntConst>(flag));
  auto result = std::static_pointer_cast<AstNode>(
      std::make_shared<BitwiseContainsNode>(haystack, needle, state));
  return result;
}

static std::shared_ptr<AstNode> parseRawFlag(ParseState &state) throw(
    ParseError) {
  state.parseCharInSpace('(');
  auto needle = AstNode::parse(state);
  if (needle->type() != INT) {
    throw ParseError(state.where(), "Type mismatch.");
  }
  state.parseCharInSpace(')');
  std::vector<std::shared_ptr<AstNode>> args;
  auto haystack =
      std::static_pointer_cast<AstNode>(std::make_shared<ConstIntFunctionNode>(
          "bamql_flags", std::move(args), state));
  auto result = std::static_pointer_cast<AstNode>(
      std::make_shared<BitwiseContainsNode>(haystack, needle, state));
  return result;
}

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
    { "chr",
      std::bind(CheckChromosomeNode::parse, std::placeholders::_1, false) },
    { "mate_chr",
      std::bind(CheckChromosomeNode::parse, std::placeholders::_1, true) },
    { "chr_name",
      parseFunction<StrFunctionNode, const std::string &>(
          "bamql_chr", { false_arg }, "Read not mapped.") },
    { "mate_chr_name",
      parseFunction<StrFunctionNode, const std::string &>(
          "bamql_chr", { true_arg }, "Read's mate not mapped.") },

    // Flags
    { "duplicate?", std::bind(parseFlag, std::placeholders::_1, BAM_FDUP) },
    { "failed_qc?", std::bind(parseFlag, std::placeholders::_1, BAM_FQCFAIL) },
    { "mapped_to_reverse?",
      std::bind(parseFlag, std::placeholders::_1, BAM_FREVERSE) },
    { "mate_mapped_to_reverse?",
      std::bind(
          parseFlag, std::placeholders::_1, BAM_FPAIRED | BAM_FMREVERSE) },
    { "mate_unmapped?",
      std::bind(parseFlag, std::placeholders::_1, BAM_FPAIRED | BAM_FMUNMAP) },
    { "paired?", std::bind(parseFlag, std::placeholders::_1, BAM_FPAIRED) },
    { "proper_pair?",
      std::bind(
          parseFlag, std::placeholders::_1, BAM_FPAIRED | BAM_FPROPER_PAIR) },
    { "raw_flag", parseRawFlag },
    { "read1?",
      std::bind(parseFlag, std::placeholders::_1, BAM_FPAIRED | BAM_FREAD1) },
    { "read2?",
      std::bind(parseFlag, std::placeholders::_1, BAM_FPAIRED | BAM_FREAD2) },
    { "secondary?",
      std::bind(parseFlag, std::placeholders::_1, BAM_FSECONDARY) },
    { "supplementary?",
      std::bind(parseFlag, std::placeholders::_1, BAM_FSUPPLEMENTARY) },
    { "unmapped?", std::bind(parseFlag, std::placeholders::_1, BAM_FUNMAP) },
    { "flags", parseFunction<ConstIntFunctionNode>("bamql_flags", {}) },

    // Constants
    { "false", [](ParseState &state) { return falseNode; } },
    { "true", [](ParseState &state) { return trueNode; } },

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

    { "mate_begin",
      parseFunction<ConstIntFunctionNode>("bamql_mate_position_begin", {}) },

    // Miscellaneous
    { "bed", parseBED },
    { "mapping_quality",
      parseFunction<BoolFunctionNode>("bamql_check_mapping_quality",
                                      { mapping_quality_arg }) },
    { "max", parseMax },
    { "min", parseMin },
    { "header",
      parseFunction<StrFunctionNode, const std::string &>(
          "bamql_header", {}, "Header not available.") },
    { "insert_size",
      parseFunction<ConstIntFunctionNode>("bamql_insert_size", {}) },
    { "nt",
      parseFunction<BoolFunctionNode>("bamql_check_nt",
                                      { int_arg, nucleotide_arg, false_arg }) },
    { "nt_exact",
      parseFunction<BoolFunctionNode>("bamql_check_nt",
                                      { int_arg, nucleotide_arg, true_arg }) },
    { "split_pair?",
      parseFunction<BoolFunctionNode>("bamql_check_split_pair", {}) },
    { "random",
      parseFunction<BoolFunctionNode>("bamql_randomly",
                                      { fixed_probability_arg }) },
  };
}
}
