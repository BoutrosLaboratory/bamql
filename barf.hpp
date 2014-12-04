#pragma once
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
namespace barf {

/**
 * Get the LLVM type for a BAM read.
 */
llvm::Type *getBamType(llvm::Module *module);

/**
 * The exception thrown when a parse error occurs.
 */
class parse_error : public std::runtime_error {
public:
parse_error(size_t offset, std::string what);
/**
 * The position in the parse string where the error occured.
 */
size_t where();
private:
size_t index;
};

class ast_node;

/**
 * A predicate is a function that parses a named predicate, and, upon success, returns a syntax node.
 */
typedef std::function<std::shared_ptr<ast_node>(const std::string &input, size_t &index) throw (parse_error)> predicate;

/**
 * A collection of predicates, where the name is the keyword in the query indicating which predicate is selected.
 */
typedef std::map<std::string, predicate> predicate_map;

/**
 * Get a map of the predicates as included in the library. These should be as described in the documenation.
 */
predicate_map getDefaultPredicates();

/**
 * An abstract syntax node representing a predicate or logical operation.
 */
class ast_node {
public:
/**
 * Parse a string into a syntax tree using the built-in logical operations and the predicates provided.
 */
static std::shared_ptr<ast_node> parse(std::string input, predicate_map predicates);
/**
 * Render this syntax node to LLVM.
 * @param read: A reference to the BAM read.
 * @returns: A boolean value indicating success or failure of this node.
 */
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<> builder, llvm::Value *read) = 0;
};

/**
 * An abstract syntax node encompassing logical ANDs and ORs that can short-circuit.
 */
class short_circuit_node : public ast_node {
public:
short_circuit_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>);
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<> builder, llvm::Value *read);
/**
 * The value that causes short circuting.
 */
virtual llvm::Value *branchValue() = 0;
private:
std::shared_ptr<ast_node>left;
std::shared_ptr<ast_node>right;
};
/**
 * A syntax node for logical conjunction (AND).
 */
class and_node : public short_circuit_node {
public:
and_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>);
virtual llvm::Value *branchValue();
};
/**
 * A syntax node for logical disjunction (OR).
 */
class or_node : public short_circuit_node {
public:
or_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>);
virtual llvm::Value *branchValue();
};
/**
 * A syntax node for logical complement (NOT).
 */
class not_node : public ast_node {
public:
not_node(std::shared_ptr<ast_node>expr);
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<> builder, llvm::Value *read);
private:
std::shared_ptr<ast_node>expr;
};
/**
 * A syntax node for ternary if.
 */
class conditional_node : public ast_node {
public:
conditional_node(std::shared_ptr<ast_node>condition, std::shared_ptr<ast_node>then_part, std::shared_ptr<ast_node> else_part);
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<> builder, llvm::Value *read);
private:
std::shared_ptr<ast_node>condition;
std::shared_ptr<ast_node>then_part;
std::shared_ptr<ast_node>else_part;
};

/**
 * A function to parse a valid non-empty integer.
 */
int parse_int(std::string input, size_t &index) throw (parse_error);
/**
 * A function to parse a valid non-empty floating point value.
 */
double parse_double(std::string input, size_t &index) throw (parse_error);
/**
 * A function to parse a non-empty string.
 * @param accept_chars: A list of valid characters that may be present in the string.
 * @param reject: If true, this inverts the meaning of `accept_chars`, accepting any character execpt those listed.
 */
std::string parse_str(std::string input, size_t &index, std::string accept_chars, bool reject=false) throw (parse_error);
/**
 * Consume whitespace in the parse stream.
 * @returns: true if any whitespace was consumed.
 */
bool parse_space(std::string input, size_t &index);
}
