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
 * Get the LLVM type for a BAM header.
 */
llvm::Type *getBamHeaderType(llvm::Module *module);

/**
 * The exception thrown when a parse error occurs.
 */
class ParseError : public std::runtime_error {
public:
  ParseError(size_t index, std::string what);
  /**
   * The position in the parse string where the error occured.
   */
  size_t where();

private:
  size_t index;
};

class AstNode;

/**
 * A predicate is a function that parses a named predicate, and, upon success,
 * returns a syntax node.
 */
typedef std::function<std::shared_ptr<AstNode>(
    const std::string &input, size_t &index) throw(ParseError)> Predicate;

/**
 * A collection of predicates, where the name is the keyword in the query
 * indicating which predicate is selected.
 */
typedef std::map<std::string, Predicate> PredicateMap;

/**
 * Get a map of the predicates as included in the library. These should be as
 * described in the documenation.
 */
PredicateMap getDefaultPredicates();

typedef llvm::Value *(barf::AstNode::*GenerateMember)(
    llvm::Module *module,
    llvm::IRBuilder<> &builder,
    llvm::Value *read,
    llvm::Value *header);

/**
 * An abstract syntax node representing a predicate or logical operation.
 */
class AstNode {
public:
  /**
   * Parse a string into a syntax tree using the built-in logical operations and
   * the predicates provided.
   */
  static std::shared_ptr<AstNode> parse(
      const std::string &input, PredicateMap predicates) throw(ParseError);
  static std::shared_ptr<AstNode> parseWithLogging(const std::string &input,
                                                   PredicateMap predicates);

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
  virtual llvm::Value *generateIndex(llvm::Module *module,
                                     llvm::IRBuilder<> &builder,
                                     llvm::Value *chromosome,
                                     llvm::Value *header);
  /**
   * Generate the LLVM function from the query.
   */
  llvm::Function *createFilterFunction(llvm::Module *module,
                                       llvm::StringRef name);
  llvm::Function *createIndexFunction(llvm::Module *module,
                                      llvm::StringRef name);

private:
  llvm::Function *createFunction(llvm::Module *module,
                                 llvm::StringRef name,
                                 llvm::StringRef param_name,
                                 llvm::Type *param_type,
                                 barf::GenerateMember member);
};

/**
 * An abstract syntax node encompassing logical ANDs and ORs that can
 * short-circuit.
 */
class ShortCircuitNode : public AstNode {
public:
  ShortCircuitNode(std::shared_ptr<AstNode> left, std::shared_ptr<AstNode>);
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header);
  virtual llvm::Value *generateIndex(llvm::Module *module,
                                     llvm::IRBuilder<> &builder,
                                     llvm::Value *read,
                                     llvm::Value *header);
  /**
   * The value that causes short circuting.
   */
  virtual llvm::Value *branchValue() = 0;

private:
  llvm::Value *generateGeneric(GenerateMember member,
                               llvm::Module *module,
                               llvm::IRBuilder<> &builder,
                               llvm::Value *param,
                               llvm::Value *header);
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};
/**
 * A syntax node for logical conjunction (AND).
 */
class AndNode : public ShortCircuitNode {
public:
  AndNode(std::shared_ptr<AstNode> left, std::shared_ptr<AstNode>);
  virtual llvm::Value *branchValue();
};
/**
 * A syntax node for logical disjunction (OR).
 */
class OrNode : public ShortCircuitNode {
public:
  OrNode(std::shared_ptr<AstNode> left, std::shared_ptr<AstNode>);
  virtual llvm::Value *branchValue();
};
/**
 * A syntax node for logical complement (NOT).
 */
class NotNode : public AstNode {
public:
  NotNode(std::shared_ptr<AstNode> expr);
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header);
  virtual llvm::Value *generateIndex(llvm::Module *module,
                                     llvm::IRBuilder<> &builder,
                                     llvm::Value *read,
                                     llvm::Value *header);

private:
  std::shared_ptr<AstNode> expr;
};
/**
 * A syntax node for ternary if.
 */
class ConditionalNode : public AstNode {
public:
  ConditionalNode(std::shared_ptr<AstNode> condition,
                  std::shared_ptr<AstNode> then_part,
                  std::shared_ptr<AstNode> else_part);
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header);
  virtual llvm::Value *generateIndex(llvm::Module *module,
                                     llvm::IRBuilder<> &builder,
                                     llvm::Value *read,
                                     llvm::Value *header);

private:
  std::shared_ptr<AstNode> condition;
  std::shared_ptr<AstNode> then_part;
  std::shared_ptr<AstNode> else_part;
};

/**
 * A function to parse a valid non-empty integer.
 */
int parseInt(const std::string &input, size_t &index) throw(ParseError);
/**
 * A function to parse a valid non-empty floating point value.
 */
double parseDouble(const std::string &input, size_t &index) throw(ParseError);
/**
 * A function to parse a non-empty string.
 * @param accept_chars: A list of valid characters that may be present in the
 * string.
 * @param reject: If true, this inverts the meaning of `accept_chars`, accepting
 * any character execpt those listed.
 */
std::string parseStr(const std::string &input,
                     size_t &index,
                     const std::string &accept_chars,
                     bool reject = false) throw(ParseError);
/**
 * Consume whitespace in the parse stream.
 * @returns: true if any whitespace was consumed.
 */
bool parseSpace(const std::string &input, size_t &index);
/**
 * Consume the specified character with optional whitespace before and after.
 */
void parseCharInSpace(const std::string &input,
                      size_t &index,
                      char c) throw(ParseError);
/**
 * Attempt to parse the supplied keyword, returing true if it could be parsed.
 */
bool parseKeyword(const std::string &input,
                  size_t &index,
                  const std::string &keyword);
/**
 * Parse an IUPAC nucleotide and return a BAM-compatible bitmap of nucleotide
 * possibilities.
 */
unsigned char parseNucleotide(const std::string &input, size_t &index);
/**
 * This helper function puts a string into a global constant and then
 * returns a pointer to it.
 *
 * One would think this is trivial, but it isn't.
 */
llvm::Value *createString(llvm::Module *module, std::string str);
}
