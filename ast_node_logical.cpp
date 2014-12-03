#include "barf.hpp"

barf::short_circuit_node::short_circuit_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>) {
	this->left = left;
	this->right = right;
}

llvm::Value *barf::short_circuit_node::generate(llvm::IRBuilder<> builder, llvm::Value *read) {
	/* Create two basic blocks for the possibly executed right-hand expression and the final block. */
	auto function = builder.GetInsertBlock()->getParent();
	auto next_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "next", function);
	auto merge_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "merge", function);

	/* Generate the left expression in the current block. */
	auto left_value = this->left->generate(builder, read);
	auto short_circuit_value = builder.CreateICmpEQ(left_value, this->branchValue());
	/* If short circuiting, jump to the final block, otherwise, do the right-hand expression. */
	builder.CreateCondBr(short_circuit_value, merge_block, next_block);
	auto original_block = builder.GetInsertBlock();

	/* Generate the right-hand expression, then jump to the final block.*/
	builder.SetInsertPoint(next_block);
	auto right_value = this->right->generate(builder, read);
	builder.CreateBr(merge_block);
	next_block = builder.GetInsertBlock();

	/* Merge from the above paths, selecting the correct value through a PHI node. */
	builder.SetInsertPoint(merge_block);
	auto phi = builder.CreatePHI(llvm::Type::getInt1Ty(llvm::getGlobalContext()), 2);
	phi->addIncoming(left_value, original_block);
	phi->addIncoming(right_value, next_block);
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

barf::conditional_node::conditional_node(std::shared_ptr<ast_node>condition, std::shared_ptr<ast_node>then_part, std::shared_ptr<ast_node> else_part) {
	this->condition = condition;
	this->then_part = then_part;
	this->else_part = else_part;
}

llvm::Value *barf::conditional_node::generate(llvm::IRBuilder<> builder, llvm::Value *read) {
	/* Create three blocks: one for the “then”, one for the “else” and one for the final. */
	auto function = builder.GetInsertBlock()->getParent();
	auto then_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "then", function);
	auto else_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "else", function);
	auto merge_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "merge", function);

	/* Compute the conditional argument and then decide to which block to jump. */
	auto conditional_result = condition->generate(builder, read);
	builder.CreateCondBr(conditional_result, then_block, else_block);

	/* Generate the “then” block. */
	builder.SetInsertPoint(then_block);
	auto then_result = then_part->generate(builder, read);
	/* Jump to the final block. */
	builder.CreateBr(merge_block);
	then_block = builder.GetInsertBlock();

	/* Generate the “else” block. */
	builder.SetInsertPoint(else_block);
	auto else_result = else_part->generate(builder, read);
	/* Jump to the final block. */
	builder.CreateBr(merge_block);
	else_block = builder.GetInsertBlock();

	/* Get the two results and select the correct one using a PHI node. */
	builder.SetInsertPoint(merge_block);
	auto phi = builder.CreatePHI(llvm::Type::getInt1Ty(llvm::getGlobalContext()), 2);
	phi->addIncoming(then_result, then_block);
	phi->addIncoming(else_result, else_block);
	return phi;
}
