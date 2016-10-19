/*
 * Copyright 2015 Paul Boutros. For details, see COPYING. Our lawyer cats sez:
 *
 * OICR makes no representations whatsoever as to the SOFTWARE contained
 * herein.  It is experimental in nature and is provided WITHOUT WARRANTY OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE OR ANY OTHER WARRANTY,
 * EXPRESS OR IMPLIED. OICR MAKES NO REPRESENTATION OR WARRANTY THAT THE USE OF
 * THIS SOFTWARE WILL NOT INFRINGE ANY PATENT OR OTHER PROPRIETARY RIGHT.  By
 * downloading this SOFTWARE, your Institution hereby indemnifies OICR against
 * any loss, claim, damage or liability, of whatsoever kind or nature, which
 * may arise from your Institution's respective use, handling or storage of the
 * SOFTWARE. If publications result from research using this SOFTWARE, we ask
 * that the Ontario Institute for Cancer Research be acknowledged and/or
 * credit be given to OICR scientists, as scientifically appropriate.
 */

#pragma once
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <llvm/Config/llvm-config.h>
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4
#include <llvm/DIBuilder.h>
#else
#include <llvm/IR/DIBuilder.h>
#endif
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

#define BAMQL_COMPILER_API_VERSION 1
namespace bamql {

/**
 * Get the LLVM type for a BAM read.
 */
llvm::Type *getBamType(llvm::Module *module);

/**
 * Get the LLVM type for a BAM header.
 */
llvm::Type *getBamHeaderType(llvm::Module *module);

llvm::Type *getErrorHandlerType(llvm::Module *module);

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

enum ExprType { BOOL, INT, FP, STR };

llvm::Type *getReifiedType(ExprType, llvm::LLVMContext &);

class AstNode;
class ParseState;
class GenerateState;

/**
 * A predicate is a function that parses a named predicate, and, upon success,
 * returns a syntax node.
 */
typedef std::function<
    std::shared_ptr<AstNode>(ParseState &state) throw(ParseError)> Predicate;

typedef std::function<llvm::Value *(GenerateState &state)> RegularExpression;

/**
 * A collection of predicates, where the name is the keyword in the query
 * indicating which predicate is selected.
 */
typedef std::map<const std::string, Predicate> PredicateMap;

/**
 * Get a map of the predicates as included in the library. These should be as
 * described in the documenation.
 */
PredicateMap getDefaultPredicates();

/**
 * Create a regular expression for a glob.
 */
RegularExpression globToRegEx(const std::string &prefix,
                              const std::string &glob_str,
                              const std::string &suffix) throw(ParseError);
/**
 * Create a regular expression for a set of strings.
 */
RegularExpression setToRegEx(const std::string &prefix,
                             const std::set<std::string> &names,
                             const std::string &suffix) throw(ParseError);

class Generator {
public:
  Generator(llvm::Module *module, llvm::DIScope *debug_scope);

  llvm::Module *module() const;
  llvm::DIScope *debugScope() const;
  void setDebugScope(llvm::DIScope *);
  llvm::Constant *createString(const std::string &str);

private:
  llvm::Module *mod;
  llvm::DIScope *debug_scope;
  std::map<std::string, llvm::Constant *> constant_pool;
};
class GenerateState {
public:
  GenerateState(std::shared_ptr<Generator> &generator, llvm::BasicBlock *entry);

  llvm::IRBuilder<> *operator*();
  llvm::IRBuilder<> *operator->();
  llvm::Module *module() const;
  llvm::DIScope *debugScope() const;
  /**
   * This helper function puts a string into a global constant and then
   * returns a pointer to it.
   *
   * One would think this is trivial, but it isn't.
   */
  llvm::Constant *createString(const std::string &str);
  std::map<void *, llvm::Value *> definitions;
  std::map<void *, llvm::Value *> definitionsIndex;

private:
  std::shared_ptr<Generator> generator;
  llvm::IRBuilder<> builder;
  llvm::DIScope *debug_scope = nullptr;
};
typedef llvm::Value *(bamql::AstNode::*GenerateMember)(GenerateState &state,
                                                       llvm::Value *param,
                                                       llvm::Value *header,
                                                       llvm::Value *error_fn,
                                                       llvm::Value *error_ctx);

typedef llvm::Value *(llvm::IRBuilder<>::*CreateICmp)(llvm::Value *lhs,
                                                      llvm::Value *rhs,
                                                      const llvm::Twine &name);
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 7
typedef llvm::Value *(llvm::IRBuilder<>::*CreateFCmp)(llvm::Value *lhs,
                                                      llvm::Value *rhs,
                                                      const llvm::Twine &name);
#else
typedef llvm::Value *(llvm::IRBuilder<>::*CreateFCmp)(llvm::Value *lhs,
                                                      llvm::Value *rhs,
                                                      const llvm::Twine &name,
                                                      llvm::MDNode *fpmathtag);
#endif

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
      const std::string &input, PredicateMap &predicates) throw(ParseError);
  static std::shared_ptr<AstNode> parseWithLogging(const std::string &input,
                                                   PredicateMap &predicates);
  /**
   * Parse from a parser state. This is useful for embedding in a larger
   * grammar.
   */
  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError);

  /**
   * Render this syntax node to LLVM.
   * @param read: A reference to the BAM read.
   * @param header: A reference to the BAM header.
   * @returns: A boolean value indicating success or failure of this node.
   */
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::Value *error_fn,
                                llvm::Value *error_ctx) = 0;
  /**
   * Render this syntax node to LLVM for the purpose of deciding how to access
   * the index.
   * @param chromosome: A reference to the chromosome taget ID.
   * @param header: A reference to the BAM header.
   * @returns: A boolean value indicating success or failure of this node.
   */
  virtual llvm::Value *generateIndex(GenerateState &state,
                                     llvm::Value *chromosome,
                                     llvm::Value *header,
                                     llvm::Value *error_fn,
                                     llvm::Value *error_ctx);
  /**
   * Determine if this node uses the BAM index (i.e., will the result of
   * `generateIndex` be non-constant).
   */
  virtual bool usesIndex();
  /**
   * Generate the LLVM function from the query.
   */
  llvm::Function *createFilterFunction(std::shared_ptr<Generator> &generator,
                                       llvm::StringRef name);
  llvm::Function *createIndexFunction(std::shared_ptr<Generator> &generator,
                                      llvm::StringRef name);

  /**
   * Gets the type of this expression.
   */
  virtual ExprType type() = 0;
  virtual void writeDebug(GenerateState &state) = 0;

private:
  llvm::Function *createFunction(std::shared_ptr<Generator> &generator,
                                 llvm::StringRef name,
                                 llvm::StringRef param_name,
                                 llvm::Type *param_type,
                                 bamql::GenerateMember member);
};
/**
 * This subclass is meant for making predicates, as it automatically fills out
 * debugging information during code generation.
 */
class DebuggableNode : public AstNode {
public:
  DebuggableNode(ParseState &state);
  void writeDebug(GenerateState &state);

private:
  unsigned int line;
  unsigned int column;
};

/**
 * An abstract syntax node encompassing logical ANDs and ORs that can
 * short-circuit.
 */
class ShortCircuitNode : public AstNode {
public:
  ShortCircuitNode(const std::shared_ptr<AstNode> &left,
                   const std::shared_ptr<AstNode> &right);
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::Value *error_fn,
                                llvm::Value *error_ctx);
  virtual llvm::Value *generateIndex(GenerateState &state,
                                     llvm::Value *tid,
                                     llvm::Value *header,
                                     llvm::Value *error_fn,
                                     llvm::Value *error_ctx);
  bool usesIndex();
  ExprType type();
  /**
   * The value that causes short circuting.
   */
  virtual llvm::Value *branchValue(llvm::LLVMContext &context) = 0;

  void writeDebug(GenerateState &state);

private:
  llvm::Value *generateGeneric(GenerateMember member,
                               GenerateState &state,
                               llvm::Value *param,
                               llvm::Value *header,
                               llvm::Value *error_fn,
                               llvm::Value *error_ctx);
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};
/**
 * A syntax node for logical conjunction (AND).
 */
class AndNode : public ShortCircuitNode {
public:
  AndNode(const std::shared_ptr<AstNode> &left,
          const std::shared_ptr<AstNode> &right);
  virtual llvm::Value *branchValue(llvm::LLVMContext &context);
};
/**
 * A syntax node for logical disjunction (OR).
 */
class OrNode : public ShortCircuitNode {
public:
  OrNode(const std::shared_ptr<AstNode> &left,
         const std::shared_ptr<AstNode> &right);
  virtual llvm::Value *branchValue(llvm::LLVMContext &context);
};
/**
 * A syntax node for exclusive disjunction (XOR).
 */
class XOrNode : public AstNode {
public:
  XOrNode(const std::shared_ptr<AstNode> &left,
          const std::shared_ptr<AstNode> &right);
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::Value *error_fn,
                                llvm::Value *error_ctx);
  virtual llvm::Value *generateIndex(GenerateState &state,
                                     llvm::Value *tid,
                                     llvm::Value *header,
                                     llvm::Value *error_fn,
                                     llvm::Value *error_ctx);
  bool usesIndex();
  ExprType type();

  void writeDebug(GenerateState &state);

private:
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};
/**
 * A syntax node for logical complement (NOT).
 */
class NotNode : public AstNode {
public:
  NotNode(const std::shared_ptr<AstNode> &expr);
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::Value *error_fn,
                                llvm::Value *error_ctx);
  virtual llvm::Value *generateIndex(GenerateState &state,
                                     llvm::Value *tid,
                                     llvm::Value *header,
                                     llvm::Value *error_fn,
                                     llvm::Value *error_ctx);
  bool usesIndex();
  ExprType type();

  void writeDebug(GenerateState &state);

private:
  std::shared_ptr<AstNode> expr;
};
/**
 * A syntax node for ternary if.
 */
class ConditionalNode : public AstNode {
public:
  ConditionalNode(const std::shared_ptr<AstNode> &condition,
                  const std::shared_ptr<AstNode> &then_part,
                  const std::shared_ptr<AstNode> &else_part);
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::Value *error_fn,
                                llvm::Value *error_ctx);
  virtual llvm::Value *generateIndex(GenerateState &state,
                                     llvm::Value *tid,
                                     llvm::Value *header,
                                     llvm::Value *error_fn,
                                     llvm::Value *error_ctx);
  bool usesIndex();
  ExprType type();
  void writeDebug(GenerateState &state);

private:
  std::shared_ptr<AstNode> condition;
  std::shared_ptr<AstNode> then_part;
  std::shared_ptr<AstNode> else_part;
};

class UseNode;
class BindingNode : public DebuggableNode {
public:
  BindingNode(ParseState &state);
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *read,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx);

  void parse(ParseState &state) throw(ParseError);

  ExprType type();

private:
  std::vector<std::shared_ptr<UseNode>> definitions;
  std::shared_ptr<AstNode> body;
};

class RegexNode : public DebuggableNode {
public:
  RegexNode(std::shared_ptr<AstNode> &operand,
            RegularExpression &&pattern,
            ParseState &state);
  llvm::Value *generateGeneric(GenerateMember member,
                               GenerateState &state,
                               llvm::Value *read,
                               llvm::Value *header,
                               llvm::Value *error_fn,
                               llvm::Value *error_ctx);
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *read,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx);
  bool usesIndex();
  ExprType type();

private:
  std::shared_ptr<AstNode> operand;
  RegularExpression pattern;
};

class CompareFPNode : public DebuggableNode {
public:
  CompareFPNode(CreateFCmp comparator,
                std::shared_ptr<AstNode> &left,
                std::shared_ptr<AstNode> &right,
                ParseState &state);
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);
  ExprType type();

private:
  CreateFCmp comparator;
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};

class CompareIntNode : public DebuggableNode {
public:
  CompareIntNode(CreateICmp comparator,
                 std::shared_ptr<AstNode> &left,
                 std::shared_ptr<AstNode> &right,
                 ParseState &state);
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);
  ExprType type();

private:
  CreateICmp comparator;
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};

class CompareStrNode : public DebuggableNode {
public:
  CompareStrNode(CreateICmp comparator,
                 std::shared_ptr<AstNode> &left,
                 std::shared_ptr<AstNode> &right,
                 ParseState &state);
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);
  ExprType type();

private:
  CreateICmp comparator;
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};

class ParseState {
public:
  ParseState(const std::string &input);
  unsigned int currentLine() const;
  unsigned int currentColumn() const;
  std::string createRuntimeError(const std::string &message);
  bool empty() const;
  void next();
  /**
   * A function to parse a valid non-empty integer.
   */
  int parseInt() throw(ParseError);
  /**
   * A function to parse a valid non-empty floating point value.
   */
  double parseDouble() throw(ParseError);
  /**
   * Parse a literal value.
   */
  std::shared_ptr<AstNode> parseLiteral() throw(ParseError);
  /**
   * Parse a predicate's arguments.
   */
  std::shared_ptr<AstNode> parsePredicate() throw(ParseError);
  /**
   * A function to parse a non-empty string.
   * @param accept_chars: A list of valid characters that may be present in the
   * string.
   * @param reject: If true, this inverts the meaning of `accept_chars`,
   * accepting
   * any character execpt those listed.
   */
  std::string parseStr(const std::string &accept_chars,
                       bool reject = false) throw(ParseError);
  /**
   * Consume whitespace in the parse stream.
   * @returns: true if any whitespace was consumed.
   */
  bool parseSpace();
  /**
   * Consume the specified character with optional whitespace before and after.
   */
  void parseCharInSpace(char c) throw(ParseError);
  /**
   * Attempt to parse the supplied keyword, returing true if it could be parsed.
   */
  bool parseKeyword(const std::string &keyword);
  /**
   * Parse an IUPAC nucleotide and return a BAM-compatible bitmap of nucleotide
   * possibilities.
   */
  unsigned char parseNucleotide();
  /**
   * Match a PCRE regular expression without any group captures.
   */
  RegularExpression parseRegEx() throw(ParseError);

  /**
  * Return the substring starting from the position to provided to the current
  * position in the input string.
  */
  std::string strFrom(size_t start) const;

  /**
  * Obtain the current position in the input string.
  */
  size_t where() const;

  /**
   * Get the current character in the input string.
   */
  char operator*() const;

  /**
   * Add a new predicate map to the current stack.
   */
  void push(const PredicateMap &map);
  /**
   * Remove a predicate map from the stack.
   */
  void pop(const PredicateMap &map);

private:
  const std::string &input;
  size_t index;
  unsigned int line;
  unsigned int column;
  std::vector<std::reference_wrapper<const PredicateMap>> predicates;
};

/**
 * The current version of the library.
 */
std::string version();
}
