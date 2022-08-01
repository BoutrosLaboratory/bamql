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
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <map>
#include <memory>
#include <set>

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

/**
 *Get the function type for an error callback.
 */
llvm::FunctionType *getErrorHandlerFunctionType(llvm::Module *module);
/**
 * Get the type for a pointer to an error handler callback.
 */
llvm::Type *getErrorHandlerType(llvm::Module *module);
llvm::FunctionType *getErrorHandlerFunctionType(llvm::Module *module);

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
typedef std::function<std::shared_ptr<AstNode>(ParseState &state)> Predicate;

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
                              const std::string &suffix);
/**
 * Create a regular expression for a set of strings.
 */
RegularExpression setToRegEx(const std::string &prefix,
                             const std::set<std::string> &names,
                             const std::string &suffix);

class Generator {
public:
  Generator(llvm::Module *module, llvm::DIScope *debug_scope);
  ~Generator();

  llvm::Module *module() const;
  llvm::DIScope *debugScope() const;
  void setDebugScope(llvm::DIScope *);
  llvm::Constant *createString(const std::string &str);
  llvm::IRBuilder<> *constructor() const;
  llvm::IRBuilder<> *destructor() const;

private:
  llvm::Module *mod;
  llvm::DIScope *debug_scope;
  std::map<std::string, llvm::Constant *> constant_pool;
  llvm::IRBuilder<> *ctor;
  llvm::IRBuilder<> *dtor;
};
class GenerateState {
public:
  GenerateState(std::shared_ptr<Generator> &generator, llvm::BasicBlock *entry);

  llvm::IRBuilder<> *operator*();
  llvm::IRBuilder<> *operator->();
  Generator &getGenerator() const;
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
};
typedef llvm::Value *(bamql::AstNode::*GenerateMember)(GenerateState &state,
                                                       llvm::Value *param,
                                                       llvm::Value *header,
                                                       llvm::Value *error_fn,
                                                       llvm::Value *error_ctx);

/**
 * An abstract syntax node representing a predicate or logical operation.
 */
class AstNode {
public:
  /**
   * Parse a string into a syntax tree using the built-in logical operations and
   * the predicates provided.
   */
  static std::shared_ptr<AstNode> parse(const std::string &input,
                                        PredicateMap &predicates);
  static std::shared_ptr<AstNode> parseWithLogging(const std::string &input,
                                                   PredicateMap &predicates);
  /**
   * Parse from a parser state. This is useful for embedding in a larger
   * grammar.
   */
  static std::shared_ptr<AstNode> parse(ParseState &state);

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
  int parseInt();
  /**
   * A function to parse a valid non-empty floating point value.
   */
  double parseDouble();
  /**
   * Parse a literal value.
   */
  std::shared_ptr<AstNode> parseLiteral();
  /**
   * Parse a predicate's arguments.
   */
  std::shared_ptr<AstNode> parsePredicate();
  /**
   * A function to parse a non-empty string.
   * @param accept_chars: A list of valid characters that may be present in the
   * string.
   * @param reject: If true, this inverts the meaning of `accept_chars`,
   * accepting
   * any character execpt those listed.
   */
  std::string parseStr(const std::string &accept_chars, bool reject = false);
  /**
   * Consume whitespace in the parse stream.
   * @returns: true if any whitespace was consumed.
   */
  bool parseSpace();
  /**
   * Consume the specified character with optional whitespace before and after.
   */
  void parseCharInSpace(char c);
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
  RegularExpression parseRegEx();

  /**
   * Match a PCRE regular expression putting named captures in the provided map.
   */
  RegularExpression parseRegEx(std::map<std::string, int> &);

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
 * Create a logical OR from a list of nodes.
 */
std::shared_ptr<AstNode> makeOr(std::vector<std::shared_ptr<AstNode>> &&terms);
/**
 * Create a logical AND from a list of nodes.
 */
std::shared_ptr<AstNode> makeAnd(std::vector<std::shared_ptr<AstNode>> &&terms);

/**
 * The current version of the library.
 */
std::string version();
} // namespace bamql
namespace std {
std::shared_ptr<bamql::AstNode> operator&(std::shared_ptr<bamql::AstNode>,
                                          std::shared_ptr<bamql::AstNode>);
std::shared_ptr<bamql::AstNode> operator|(std::shared_ptr<bamql::AstNode>,
                                          std::shared_ptr<bamql::AstNode>);
std::shared_ptr<bamql::AstNode> operator^(std::shared_ptr<bamql::AstNode>,
                                          std::shared_ptr<bamql::AstNode>);
std::shared_ptr<bamql::AstNode> operator~(std::shared_ptr<bamql::AstNode>);
} // namespace std
