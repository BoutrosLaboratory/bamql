#include <cstdint>
#include <htslib/sam.h>
#include "barf.hpp"
#include "boolean_constant.hpp"
#include "check_aux.hpp"
#include "check_chromosome.hpp"
#include "check_flag.hpp"

// Please keep the predicates in alphabetical order.
namespace barf {

/* This insanity is brought to you by samtools's sam_tview.c */
static bool readGroupChar(char input, bool not_first) {
  return input >= '!' && input <= '~' && (not_first || input != '=');
}

/**
 * A predicate that is true if the mapping quality is sufficiently good.
 */
class MappingQualityNode : public AstNode {
public:
  MappingQualityNode(double probability_) : probability(probability_) {}
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

  static std::shared_ptr<AstNode> parse(const std::string &input,
                                        size_t &index) throw(ParseError) {
    parseCharInSpace(input, index, '(');

    auto probability = parseDouble(input, index);
    if (probability <= 0 || probability >= 1) {
      throw ParseError(index, "The provided probability is not probable.");
    }

    parseCharInSpace(input, index, ')');

    return std::make_shared<MappingQualityNode>(probability);
  }

private:
  double probability;
};

/**
 * A predicate that randomly is true.
 */
class RandomlyNode : public AstNode {
public:
  RandomlyNode(double probability_) : probability(probability_) {}
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

  static std::shared_ptr<AstNode> parse(const std::string &input,
                                        size_t &index) throw(ParseError) {
    parseCharInSpace(input, index, '(');

    auto probability = parseDouble(input, index);
    if (probability < 0 || probability > 1) {
      throw ParseError(index, "The provided probability is not probable.");
    }

    parseCharInSpace(input, index, ')');

    return std::make_shared<RandomlyNode>(probability);
  }

private:
  double probability;
};

/**
 * A predicate that check mapped positions.
 */
class PositionNode : public AstNode {
public:
  PositionNode(int32_t start_, int32_t end_) : start(start_), end(end_) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = module->getFunction("check_position");
    return builder.CreateCall3(
        function,
        read,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm::getGlobalContext()),
                               start),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm::getGlobalContext()),
                               end));
  }

  static std::shared_ptr<AstNode> parse(const std::string &input,
                                        size_t &index) throw(ParseError) {
    parseCharInSpace(input, index, '(');
    auto start = parseInt(input, index);
    parseCharInSpace(input, index, ',');
    auto end = parseInt(input, index);
    parseCharInSpace(input, index, ')');
    return std::make_shared<PositionNode>(start, end);
  }
  static std::shared_ptr<AstNode> parseAfter(const std::string &input,
                                             size_t &index) throw(ParseError) {
    parseCharInSpace(input, index, '(');
    auto pos = parseInt(input, index);
    parseCharInSpace(input, index, ')');
    return std::make_shared<PositionNode>(pos, INT32_MAX);
  }
  static std::shared_ptr<AstNode> parseBefore(const std::string &input,
                                              size_t &index) throw(ParseError) {
    parseCharInSpace(input, index, '(');
    auto pos = parseInt(input, index);
    parseCharInSpace(input, index, ')');
    return std::make_shared<PositionNode>(0, pos);
  }

private:
  int32_t start;
  int32_t end;
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
    { std::string("mate_mapped_to_reverse?"), CheckFlag<BAM_FMREVERSE>::parse },
    { std::string("mate_unmapped?"), CheckFlag<BAM_FMUNMAP>::parse },
    { std::string("paired?"), CheckFlag<BAM_FPAIRED>::parse },
    { std::string("proper_pair?"), CheckFlag<BAM_FPROPER_PAIR>::parse },
    { std::string("read1?"), CheckFlag<BAM_FREAD1>::parse },
    { std::string("read2?"), CheckFlag<BAM_FREAD2>::parse },
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
    { std::string("random"), RandomlyNode::parse }
  };
}
}
