#pragma once
#include <htslib/sam.h>
#include "barf.hpp"

namespace barf {
typedef llvm::ConstantInt *(*BoolConstant)(llvm::LLVMContext &);

/**
 * A predicate that always returns a constant.
 */
template <BoolConstant CF> class ConstantNode : public AstNode {
public:
  llvm::Value *generate(llvm::Module *module,
                        llvm::IRBuilder<> &builder,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::DIScope *debug_scope) {
    return CF(llvm::getGlobalContext());
  }
  llvm::Value *generateIndex(llvm::Module *module,
                             llvm::IRBuilder<> &builder,
                             llvm::Value *tid,
                             llvm::Value *header,
                             llvm::DIScope *debug_scope) {
    return CF(llvm::getGlobalContext());
  }
  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    static auto result = std::make_shared<ConstantNode<CF>>();
    return result;
  }
  void writeDebug(llvm::Module *module,
                  llvm::IRBuilder<> &builder,
                  llvm::DIScope *debug_scope) {}
};
}
