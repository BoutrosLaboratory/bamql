#include <htslib/sam.h>
#include "barf.hpp"

namespace barf {

/**
 * A predicate that checks of the read's flag.
 */
template <unsigned int F> class CheckFlag : public AstNode {
public:
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = module->getFunction("check_flag");
    return builder.CreateCall2(
        function,
        read,
        llvm::ConstantInt::get(llvm::Type::getInt16Ty(llvm::getGlobalContext()),
                               F));
  }

  static std::shared_ptr<AstNode> parse(const std::string &input,
                                        size_t &index) throw(ParseError) {
    static auto result = std::make_shared<CheckFlag<F>>();
    return result;
  }
};
}
