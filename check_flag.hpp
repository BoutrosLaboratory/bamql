#include <htslib/sam.h>
#include "barf.hpp"

namespace barf {

/**
 * A predicate that checks of the read's flag.
 */
template <unsigned int F> class CheckFlag : public DebuggableNode {
public:
  CheckFlag(ParseState &state) : DebuggableNode(state) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::DIScope *debug_scope) {
    auto function = module->getFunction("check_flag");
    return builder.CreateCall2(
        function,
        read,
        llvm::ConstantInt::get(llvm::Type::getInt16Ty(llvm::getGlobalContext()),
                               F));
  }

  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    static auto result = std::make_shared<CheckFlag<F>>(state);
    return result;
  }
};
}
