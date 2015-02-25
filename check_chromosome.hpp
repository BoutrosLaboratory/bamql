#include <htslib/sam.h>
#include "barf.hpp"

namespace barf {

/**
 * A predicate that checks of the chromosome name.
 */
template <bool mate> class CheckChromosomeNode : public AstNode {
public:
  CheckChromosomeNode(std::string name_) : name(name_) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = module->getFunction("check_chromosome");
    return builder.CreateCall4(
        function,
        read,
        header,
        createString(module, name),
        mate ? llvm::ConstantInt::getTrue(llvm::getGlobalContext())
             : llvm::ConstantInt::getFalse(llvm::getGlobalContext()));
  }
  virtual llvm::Value *generateIndex(llvm::Module *module,
                                     llvm::IRBuilder<> &builder,
                                     llvm::Value *chromosome,
                                     llvm::Value *header) {
    if (mate) {
      return llvm::ConstantInt::getTrue(llvm::getGlobalContext());
    }
    auto function = module->getFunction("check_chromosome_id");
    return builder.CreateCall3(
        function, chromosome, header, createString(module, name));
  }

  static std::shared_ptr<AstNode> parse(const std::string &input,
                                        size_t &index) throw(ParseError) {
    parseCharInSpace(input, index, '(');

    auto str = parseStr(
        input,
        index,
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_*?.");
    if (str.compare(0, 3, "chr") == 0) {
      throw ParseError(index, "Chromosome names must not start with `chr'.");
    }

    parseCharInSpace(input, index, ')');

    // If we are dealing with a chromosome that goes by many names, match all of
    // them.
    if (str.compare("23") == 0 || str.compare("X") == 0 ||
        str.compare("x") == 0) {
      return std::make_shared<OrNode>(
          std::make_shared<CheckChromosomeNode<mate>>("23"),
          std::make_shared<CheckChromosomeNode<mate>>("x"));
    }

    if (str.compare("24") == 0 || str.compare("Y") == 0 ||
        str.compare("y") == 0) {
      return std::make_shared<OrNode>(
          std::make_shared<CheckChromosomeNode<mate>>("24"),
          std::make_shared<CheckChromosomeNode<mate>>("y"));
    }
    if (str.compare("25") == 0 || str.compare("M") == 0 ||
        str.compare("m") == 0) {
      return std::make_shared<OrNode>(
          std::make_shared<CheckChromosomeNode<mate>>("25"),
          std::make_shared<CheckChromosomeNode<mate>>("m"));
    }
    // otherwise, just match the provided chromosome.
    return std::make_shared<CheckChromosomeNode<mate>>(str);
  }

private:
  std::string name;
};
}
