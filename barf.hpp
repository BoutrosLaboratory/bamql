#pragma once
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

// vim: set ts=2 sw=2 tw=0 :

namespace barf {

/**
 * Get the LLVM type for a BAM read.
 */
llvm::Type *getBamType(llvm::Module *module);

/**
 * Get the LLVM type for a BAM header.
 */
llvm::Type *getBamHeaderType(llvm::Module *module);

/**
 * The exception thrown when a parse error occurs.
 */
class parse_error : public std::runtime_error {
public:
	parse_error(size_t index, std::string what);
	/**
	 * The position in the parse string where the error occured.
	 */
	size_t where();

private:
	size_t index;
};

class ast_node;

/**
 * A predicate is a function that parses a named predicate, and, upon success,
 * returns a syntax node.
 */
typedef std::function<std::shared_ptr<ast_node>(
		const std::string &input, size_t &index) throw(parse_error)> predicate;

/**
 * A collection of predicates, where the name is the keyword in the query
 * indicating which predicate is selected.
 */
typedef std::map<std::string, predicate> predicate_map;

/**
 * Get a map of the predicates as included in the library. These should be as
 * described in the documenation.
 */
predicate_map getDefaultPredicates();

class ast_node;

typedef llvm::Value *(barf::ast_node::*generate_member)(
		llvm::Module *module,
		llvm::IRBuilder<> &builder,
		llvm::Value *read,
		llvm::Value *header);

/**
 * An abstract syntax node representing a predicate or logical operation.
 */
class ast_node {
public:
	/**
	 * Parse a string into a syntax tree using the built-in logical operations and
	 * the predicates provided.
	 */
	static std::shared_ptr<ast_node> parse(
			const std::string &input, predicate_map predicates) throw(parse_error);
	/**
	 * Render this syntax node to LLVM.
	 * @param read: A reference to the BAM read.
	 * @param header: A reference to the BAM header.
	 * @returns: A boolean value indicating success or failure of this node.
	 */
	virtual llvm::Value *generate(llvm::Module *module,
																llvm::IRBuilder<> &builder,
																llvm::Value *read,
																llvm::Value *header) = 0;
	/**
	 * Render this syntax node to LLVM for the purpose of deciding how to access
	 * the index.
	 * @param chromosome: A reference to the chromosome taget ID.
	 * @param header: A reference to the BAM header.
	 * @returns: A boolean value indicating success or failure of this node.
	 */
	virtual llvm::Value *generate_index(llvm::Module *module,
																			llvm::IRBuilder<> &builder,
																			llvm::Value *chromosome,
																			llvm::Value *header);
	/**
	 * Generate the LLVM function from the query.
	 */
	llvm::Function *create_filter_function(llvm::Module *module,
																				 llvm::StringRef name);
	llvm::Function *create_index_function(llvm::Module *module,
																				llvm::StringRef name);

private:
	llvm::Function *create_function(llvm::Module *module,
																	llvm::StringRef name,
																	llvm::StringRef param_name,
																	llvm::Type *param_type,
																	barf::generate_member member);
};

/**
 * An abstract syntax node encompassing logical ANDs and ORs that can
 * short-circuit.
 */
class short_circuit_node : public ast_node {
public:
	short_circuit_node(std::shared_ptr<ast_node> left, std::shared_ptr<ast_node>);
	virtual llvm::Value *generate(llvm::Module *module,
																llvm::IRBuilder<> &builder,
																llvm::Value *read,
																llvm::Value *header);
	virtual llvm::Value *generate_index(llvm::Module *module,
																			llvm::IRBuilder<> &builder,
																			llvm::Value *read,
																			llvm::Value *header);
	/**
	 * The value that causes short circuting.
	 */
	virtual llvm::Value *branchValue() = 0;

private:
	llvm::Value *generate_generic(generate_member member,
																llvm::Module *module,
																llvm::IRBuilder<> &builder,
																llvm::Value *param,
																llvm::Value *header);
	std::shared_ptr<ast_node> left;
	std::shared_ptr<ast_node> right;
};
/**
 * A syntax node for logical conjunction (AND).
 */
class and_node : public short_circuit_node {
public:
	and_node(std::shared_ptr<ast_node> left, std::shared_ptr<ast_node>);
	virtual llvm::Value *branchValue();
};
/**
 * A syntax node for logical disjunction (OR).
 */
class or_node : public short_circuit_node {
public:
	or_node(std::shared_ptr<ast_node> left, std::shared_ptr<ast_node>);
	virtual llvm::Value *branchValue();
};
/**
 * A syntax node for logical complement (NOT).
 */
class not_node : public ast_node {
public:
	not_node(std::shared_ptr<ast_node> expr);
	virtual llvm::Value *generate(llvm::Module *module,
																llvm::IRBuilder<> &builder,
																llvm::Value *read,
																llvm::Value *header);
	virtual llvm::Value *generate_index(llvm::Module *module,
																			llvm::IRBuilder<> &builder,
																			llvm::Value *read,
																			llvm::Value *header);

private:
	std::shared_ptr<ast_node> expr;
};
/**
 * A syntax node for ternary if.
 */
class conditional_node : public ast_node {
public:
	conditional_node(std::shared_ptr<ast_node> condition,
									 std::shared_ptr<ast_node> then_part,
									 std::shared_ptr<ast_node> else_part);
	virtual llvm::Value *generate(llvm::Module *module,
																llvm::IRBuilder<> &builder,
																llvm::Value *read,
																llvm::Value *header);
	virtual llvm::Value *generate_index(llvm::Module *module,
																			llvm::IRBuilder<> &builder,
																			llvm::Value *read,
																			llvm::Value *header);

private:
	std::shared_ptr<ast_node> condition;
	std::shared_ptr<ast_node> then_part;
	std::shared_ptr<ast_node> else_part;
};

/**
 * A function to parse a valid non-empty integer.
 */
int parse_int(const std::string &input, size_t &index) throw(parse_error);
/**
 * A function to parse a valid non-empty floating point value.
 */
double parse_double(const std::string &input, size_t &index) throw(parse_error);
/**
 * A function to parse a non-empty string.
 * @param accept_chars: A list of valid characters that may be present in the
 * string.
 * @param reject: If true, this inverts the meaning of `accept_chars`, accepting
 * any character execpt those listed.
 */
std::string parse_str(const std::string &input,
											size_t &index,
											const std::string &accept_chars,
											bool reject = false) throw(parse_error);
/**
 * Consume whitespace in the parse stream.
 * @returns: true if any whitespace was consumed.
 */
bool parse_space(const std::string &input, size_t &index);
/**
 * Consume the specified character with optional whitespace before and after.
 */
void parse_char_in_space(const std::string &input,
												 size_t &index,
												 char c) throw(parse_error);
/**
 * Attempt to parse the supplied keyword, returing true if it could be parsed.
 */
bool parse_keyword(const std::string &input,
									 size_t &index,
									 const std::string &keyword);
}
