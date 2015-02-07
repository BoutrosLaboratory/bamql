#include <htslib/sam.h>
#include "barf.hpp"

// vim: set ts=2 sw=2 tw=0 :

namespace barf {
typedef llvm::ConstantInt *(*BoolConstant)(llvm::LLVMContext &);

/**
 * A predicate that always returns a constant.
 */
template <BoolConstant CF> class constant_node : public ast_node {
public:
	virtual llvm::Value *generate(llvm::Module *module,
																llvm::IRBuilder<> &builder,
																llvm::Value *read,
																llvm::Value *header) {
		return llvm::ConstantInt::getFalse(llvm::getGlobalContext());
	}
	virtual llvm::Value *generate_index(llvm::Module *module,
																			llvm::IRBuilder<> &builder,
																			llvm::Value *tid,
																			llvm::Value *header) {
		return CF(llvm::getGlobalContext());
	}
	static std::shared_ptr<ast_node> parse(const std::string &input,
																				 size_t &index) throw(parse_error) {
		static auto result = std::make_shared<constant_node<CF>>();
		return result;
	}
};
}
