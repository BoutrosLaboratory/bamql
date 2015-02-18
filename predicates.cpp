#include <htslib/sam.h>
#include "barf.hpp"
#include "boolean_constant.hpp"
#include "check_aux.hpp"
#include "check_chromosome.hpp"
#include "check_flag.hpp"

// Please keep the predicates in alphabetical order.
namespace barf {

/* This insanity is brought to you by samtools's sam_tview.c */
bool read_group_char(char input, bool not_first) {
  return input >= '!' && input <= '~' && (not_first || input != '=');
}

/**
 * A predicate that is true if the mapping quality is sufficiently good.
 */
class mapping_quality_node : public ast_node {
public:
  mapping_quality_node(double probability_) : probability(probability_) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = module->getFunction("check_mapping_quality");
    return builder.CreateCall(
        function,
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvm::getGlobalContext()),
                               -10 * log(probability)));
  }

  static std::shared_ptr<ast_node> parse(const std::string &input,
                                         size_t &index) throw(parse_error) {
    parse_char_in_space(input, index, '(');

    auto probability = parse_double(input, index);
    if (probability <= 0 || probability >= 1) {
      throw parse_error(index, "The provided probability is not probable.");
    }

    parse_char_in_space(input, index, ')');

    return std::make_shared<mapping_quality_node>(probability);
  }

private:
  double probability;
};

/**
 * A predicate that randomly is true.
 */
class randomly_node : public ast_node {
public:
  randomly_node(double probability_) : probability(probability_) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = module->getFunction("randomly");
    return builder.CreateCall(
        function,
        llvm::ConstantFP::get(llvm::Type::getDoubleTy(llvm::getGlobalContext()),
                              probability));
  }

  static std::shared_ptr<ast_node> parse(const std::string &input,
                                         size_t &index) throw(parse_error) {
    parse_char_in_space(input, index, '(');

    auto probability = parse_double(input, index);
    if (probability < 0 || probability > 1) {
      throw parse_error(index, "The provided probability is not probable.");
    }

    parse_char_in_space(input, index, ')');

    return std::make_shared<randomly_node>(probability);
  }

private:
  double probability;
};

/**
 * All the predicates known to the system.
 */
predicate_map getDefaultPredicates() {

  return {
    // Auxiliary data
    { std::string("read_group"),
      check_aux_string_node<'R', 'G', read_group_char>::parse },

    // Chromosome information
    { std::string("chr"), check_chromosome_node<false>::parse },
    { std::string("mate_chr"), check_chromosome_node<true>::parse },

    // Flags
    { std::string("duplicate?"), check_flag<BAM_FDUP>::parse },
    { std::string("failed_qc?"), check_flag<BAM_FQCFAIL>::parse },
    { std::string("mapped_to_reverse?"), check_flag<BAM_FREVERSE>::parse },
    { std::string("mate_mapped_to_reverse?"),
      check_flag<BAM_FMREVERSE>::parse },
    { std::string("mate_unmapped?"), check_flag<BAM_FMUNMAP>::parse },
    { std::string("paired?"), check_flag<BAM_FPAIRED>::parse },
    { std::string("proper_pair?"), check_flag<BAM_FPROPER_PAIR>::parse },
    { std::string("read1?"), check_flag<BAM_FREAD1>::parse },
    { std::string("read2?"), check_flag<BAM_FREAD2>::parse },
    { std::string("secondary?"), check_flag<BAM_FSECONDARY>::parse },
    { std::string("supplementary?"), check_flag<BAM_FSUPPLEMENTARY>::parse },
    { std::string("unmapped?"), check_flag<BAM_FUNMAP>::parse },

    // Constants
    { std::string("false"), constant_node<llvm::ConstantInt::getFalse>::parse },
    { std::string("true"), constant_node<llvm::ConstantInt::getTrue>::parse },

    // Miscellaneous
    { std::string("mapping_quality"), mapping_quality_node::parse },
    { std::string("random"), randomly_node::parse }
  };
}
}
