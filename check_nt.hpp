#pragma once
#include "boolean_constant.hpp"

namespace barf {
/**
 * A predicate that matches a particular nucleotide.
 */
template <BoolConstant EXACT> class NucleotideNode : public DebuggableNode {
public:
  NucleotideNode(int32_t position_, unsigned char nt_, ParseState &state)
      : DebuggableNode(state), position(position_), nt(nt_) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::DIScope *debug_scope) {
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

  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    state.parseCharInSpace('(');
    auto position = state.parseInt();
    state.parseCharInSpace(',');
    auto nt = state.parseNucleotide();
    state.parseCharInSpace(')');
    return std::make_shared<NucleotideNode<EXACT>>(position, nt, state);
  }

private:
  int32_t position;
  unsigned char nt;
};
}
