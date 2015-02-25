#include <htslib/sam.h>
#include "barf.hpp"

namespace barf {
typedef llvm::ConstantInt *(*BoolConstant)(llvm::LLVMContext &);

/**
 * A predicate that always returns a constant.
 */
template <BoolConstant CF> class ConstantNode : public AstNode {
public:
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header) {
    return CF(llvm::getGlobalContext());
  }
  virtual llvm::Value *generateIndex(llvm::Module *module,
                                     llvm::IRBuilder<> &builder,
                                     llvm::Value *tid,
                                     llvm::Value *header) {
    return CF(llvm::getGlobalContext());
  }
  static std::shared_ptr<AstNode> parse(const std::string &input,
                                        size_t &index) throw(ParseError) {
    static auto result = std::make_shared<ConstantNode<CF>>();
    return result;
  }
};
}
