#include "barf.hpp"

barf::short_circuit_node::short_circuit_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>) {
	this->left = left;
	this->right = right;
}

llvm::Value *barf::short_circuit_node::generate(llvm::IRBuilder<> builder, llvm::Value *read) {
	auto first = this->left->generate(builder, read);
  auto short_circuit = builder.CreateICmpEQ(first, this->branchValue());
	auto function = builder.GetInsertBlock()->getParent();
	auto next = llvm::BasicBlock::Create(llvm::getGlobalContext(), "next", function);
	auto merge = llvm::BasicBlock::Create(llvm::getGlobalContext(), "merge", function);

  builder.CreateCondBr(short_circuit, merge, next);
	auto original = builder.GetInsertBlock();
	builder.SetInsertPoint(merge);
	auto second = this->right->generate(builder, read);
	builder.CreateBr(merge);
	next = builder.GetInsertBlock();
	auto phi = builder.CreatePHI(llvm::Type::getInt1Ty(llvm::getGlobalContext()), 2);
	phi->addIncoming(first, original);
	phi->addIncoming(second, merge);
	return phi;
}	

barf::and_node::and_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>right) : short_circuit_node(left, right) {
}
llvm::Value *barf::and_node::branchValue() {
	return llvm::ConstantInt::getFalse(llvm::getGlobalContext());
}

barf::or_node::or_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>right) : short_circuit_node(left, right) {
}
llvm::Value *barf::or_node::branchValue() {
	return llvm::ConstantInt::getTrue(llvm::getGlobalContext());
}

barf::not_node::not_node(std::shared_ptr<ast_node>expr) {
	this->expr = expr;
}
llvm::Value *barf::not_node::generate(llvm::IRBuilder<> builder, llvm::Value *read) {
	llvm::Value *result = this->expr->generate(builder, read);
	return builder.CreateNot(result);
}

