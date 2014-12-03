#include "barf.hpp"

// This must be included exactly once in only this file!!!
#include "runtime.inc"

class true_node : public barf::ast_node {
	virtual llvm::Value *generate(llvm::IRBuilder<> builder, llvm::Value *read) {
		return llvm::ConstantInt::getTrue(llvm::getGlobalContext());
	}
};

static std::shared_ptr<barf::ast_node> parse_true(std::string input, int&index) throw (barf::parse_error) {
	static auto result = std::make_shared<true_node>();
	return result;
}

barf::predicate_map barf::getDefaultPredicates() {
	return  {
		{std::string("true"), parse_true}
	};
}
