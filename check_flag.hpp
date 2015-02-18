#include <htslib/sam.h>
#include "barf.hpp"

namespace barf {

/**
 * A predicate that checks of the read's flag.
 */
template <unsigned int F> class check_flag : public ast_node {
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

  static std::shared_ptr<ast_node> parse(const std::string &input,
                                         size_t &index) throw(parse_error) {
    static auto result = std::make_shared<check_flag<F>>();
    return result;
  }
};
}
