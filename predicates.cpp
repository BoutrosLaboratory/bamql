#include "barf.hpp"

// This must be included exactly once in only this file!!!
#include "runtime.inc"

// Please keep the predicates in alphabetical order.
namespace barf {

/**
 * A predicate that always return true.
 */
class true_node : public ast_node {
	virtual llvm::Value *generate(llvm::IRBuilder<> builder, llvm::Value *read) {
		return llvm::ConstantInt::getTrue(llvm::getGlobalContext());
	}
};

static std::shared_ptr<ast_node> parse_true(std::string input, int&index) throw (parse_error) {
	static auto result = std::make_shared<true_node>();
	return result;
}

/**
 * All the predicates known to the system.
 */
predicate_map getDefaultPredicates() {
	return  {
		{std::string("true"), parse_true}
	};
}
}
