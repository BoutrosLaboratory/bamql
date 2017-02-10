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

// Please keep the predicates in alphabetical order.
namespace bamql {

// These must be lower case
const std::set<std::set<std::string>> equivalence_sets = {
  { "23", "x" }, { "24", "y" }, { "25", "m", "mt" }
};

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

class FixedProbabilityArg : public ProbabilityArg {
public:
  FixedProbabilityArg() {}

protected:
  std::shared_ptr<AstNode> makeAst(double probability) const {
    return std::make_shared<
        LiteralNode<double, decltype(&make_dbl), &make_dbl, FP>>(probability);
  }
};
class MappingQualityArg : public ProbabilityArg {
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
static const IntArg int_flag_duplicate(BAM_FDUP);
static const IntArg int_flag_failed_qc(BAM_FQCFAIL);
static const IntArg int_flag_mapped_to_reverse(BAM_FREVERSE);
static const IntArg int_flag_mate_mapped_to_reverse(BAM_FPAIRED |
                                                    BAM_FMREVERSE);
static const IntArg int_flag_mate_unmapped(BAM_FPAIRED | BAM_FMUNMAP);
static const IntArg int_flag_paired(BAM_FPAIRED);
static const IntArg int_flag_proper_pair(BAM_FPAIRED | BAM_FPROPER_PAIR);
static const IntArg int_flag_read1(BAM_FPAIRED | BAM_FREAD1);
static const IntArg int_flag_read2(BAM_FPAIRED | BAM_FREAD2);
static const IntArg int_flag_secondary(BAM_FSECONDARY);
static const IntArg int_flag_supplementary(BAM_FSUPPLEMENTARY);
static const IntArg int_flag_unmapped(BAM_FUNMAP);
static const FixedProbabilityArg fixed_probability_arg;
static const MappingQualityArg mapping_quality_arg;

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
    { "duplicate?",
      parseFunction<BoolFunctionNode>("bamql_check_flag",
                                      { int_flag_duplicate }) },
    { "failed_qc?",
      parseFunction<BoolFunctionNode>("bamql_check_flag",
                                      { int_flag_failed_qc }) },
    { "mapped_to_reverse?",
      parseFunction<BoolFunctionNode>("bamql_check_flag",
                                      { int_flag_mapped_to_reverse }) },
    { "mate_mapped_to_reverse?",
      parseFunction<BoolFunctionNode>("bamql_check_flag",
                                      { int_flag_mate_mapped_to_reverse }) },
    { "mate_unmapped?",
      parseFunction<BoolFunctionNode>("bamql_check_flag",
                                      { int_flag_unmapped }) },
    { "paired?",
      parseFunction<BoolFunctionNode>("bamql_check_flag",
                                      { int_flag_paired }) },
    { "proper_pair?",
      parseFunction<BoolFunctionNode>("bamql_check_flag",
                                      { int_flag_proper_pair }) },
    { "raw_flag",
      parseFunction<BoolFunctionNode>("bamql_check_flag", { int_arg }) },
    { "read1?",
      parseFunction<BoolFunctionNode>("bamql_check_flag", { int_flag_read1 }) },
    { "read2?",
      parseFunction<BoolFunctionNode>("bamql_check_flag", { int_flag_read2 }) },
    { "secondary?",
      parseFunction<BoolFunctionNode>("bamql_check_flag",
                                      { int_flag_secondary }) },
    { "supplementary?",
      parseFunction<BoolFunctionNode>("bamql_check_flag",
                                      { int_flag_supplementary }) },
    { "unmapped?",
      parseFunction<BoolFunctionNode>("bamql_check_flag",
                                      { int_flag_unmapped }) },

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
