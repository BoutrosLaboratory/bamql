#pragma once
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <llvm/IR/IRBuilder.h>
namespace barf {

class parse_error : public std::runtime_error {
public:
parse_error(size_t offset, std::string what);
size_t where();
private:
size_t index;
};

class ast_node;
typedef std::function<std::shared_ptr<ast_node>(std::string input, int&index) throw (parse_error)> predicate;

typedef std::map<std::string, predicate> predicate_map;

predicate_map getDefaultPredicates();

class ast_node {
public:
static std::shared_ptr<ast_node> parse(std::string input, predicate_map predicates);

virtual llvm::Value *generate(llvm::IRBuilder<> builder, llvm::Value *read) = 0;
};

class short_circuit_node : public ast_node {
public:
short_circuit_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>);
virtual llvm::Value *generate(llvm::IRBuilder<> builder, llvm::Value *read);
virtual llvm::Value *branchValue() = 0;
private:
std::shared_ptr<ast_node>left;
std::shared_ptr<ast_node>right;
};
class and_node : public short_circuit_node {
public:
and_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>);
virtual llvm::Value *branchValue();
};
class or_node : public short_circuit_node {
public:
or_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>);
virtual llvm::Value *branchValue();
};
class not_node : public ast_node {
public:
not_node(std::shared_ptr<ast_node>expr);
virtual llvm::Value *generate(llvm::IRBuilder<> builder, llvm::Value *read);
private:
std::shared_ptr<ast_node>expr;
};
class conditional_node : public ast_node {
public:
conditional_node(std::shared_ptr<ast_node>condition, std::shared_ptr<ast_node>then_part, std::shared_ptr<ast_node> else_part);
virtual llvm::Value *generate(llvm::IRBuilder<> builder, llvm::Value *read);
private:
std::shared_ptr<ast_node>condition;
std::shared_ptr<ast_node>then_part;
std::shared_ptr<ast_node>else_part;
};

int parse_int(std::string input, int& index) throw (parse_error);
double parse_double(std::string input, int& index) throw (parse_error);
std::string parse_str(std::string input, int& index, std::string accept_chars, bool reject=false) throw (parse_error);
void parse_space(std::string input, int& index);
}
