#pragma once
#include "boolean_constant.hpp"

namespace barf {
/**
 * A predicate that matches a particular nucleotide.
 */
template <BoolConstant EXACT> class NucleotideNode : public AstNode {
public:
  NucleotideNode(int32_t position_, unsigned char nt_)
      : position(position_), nt(nt_) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = module->getFunction("check_nt");
    return builder.CreateCall4(
        function,
        read,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm::getGlobalContext()),
                               position),
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvm::getGlobalContext()),
                               nt),
        EXACT(llvm::getGlobalContext()));
  }

  static std::shared_ptr<AstNode> parse(const std::string &input,
                                        size_t &index) throw(ParseError) {
    parseCharInSpace(input, index, '(');
    auto position = parseInt(input, index);
    parseCharInSpace(input, index, ',');
    auto nt = parseNucleotide(input, index);
    parseCharInSpace(input, index, ')');
    return std::make_shared<NucleotideNode<EXACT>>(position, nt);
  }

private:
  int32_t position;
  unsigned char nt;
};
}
