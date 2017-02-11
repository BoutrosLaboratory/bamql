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

#include "bamql-compiler.hpp"
#include "ast_node_loop.hpp"

class bamql::LoopVar : public bamql::AstNode {
public:
  LoopVar(bamql::LoopNode *owner_) : owner(owner_) {}
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx) {
    return state.definitions[this];
  }
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *read,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx) {
    return llvm::ConstantInt::getTrue(state.module()->getContext());
  }
  bamql::ExprType type() { return owner->values.front()->type(); }
  void writeDebug(GenerateState &state) {}

private:
  bamql::LoopNode *owner;
};

bamql::LoopNode::LoopNode(
    ParseState &state,
    const std::string &var_name,
    bool all_,
    std::vector<std::shared_ptr<AstNode>> &&values_) throw(ParseError)
    : all(all_), values(std::move(values_)),
      var(std::make_shared<LoopVar>(this)) {
  PredicateMap loopmap{ { var_name, [&](bamql::ParseState &state) {
    return std::static_pointer_cast<AstNode>(var);
  } } };
  state.push(loopmap);
  body = AstNode::parse(state);
  if (body->type() != bamql::BOOL) {
    throw ParseError(state.where(), "Loop body expression must be Boolean.");
  }
  state.pop(loopmap);
}

llvm::Value *bamql::LoopNode::generate(bamql::GenerateState &state,
                                       llvm::Value *read,
                                       llvm::Value *header,
                                       llvm::Value *error_fn,
                                       llvm::Value *error_ctx) {

  auto type = llvm::Type::getInt32Ty(state.module()->getContext());
  auto index = state->CreateAlloca(type);
  state->CreateStore(llvm::ConstantInt::get(type, 0), index);

  auto function = state->GetInsertBlock()->getParent();
  auto body_block =
      llvm::BasicBlock::Create(state.module()->getContext(), "body", function);
  auto merge_block =
      llvm::BasicBlock::Create(state.module()->getContext(), "merge", function);
  auto next_block =
      llvm::BasicBlock::Create(state.module()->getContext(), "next", function);
  state->CreateBr(next_block);
  state->SetInsertPoint(next_block);
  auto switch_inst =
      state->CreateSwitch(state->CreateLoad(index), merge_block, values.size());
  std::map<llvm::BasicBlock *, llvm::Value *> results;
  for (size_t it = 0; it < values.size(); it++) {
    auto case_block = llvm::BasicBlock::Create(
        state.module()->getContext(), "case", function);
    switch_inst->addCase(llvm::ConstantInt::get(type, it), case_block);
    state->SetInsertPoint(case_block);
    auto result =
        values[it]->generate(state, read, header, error_fn, error_ctx);
    state->CreateBr(body_block);
    results[state->GetInsertBlock()] = result;
  }
  state->SetInsertPoint(body_block);
  auto phi =
      state->CreatePHI(results.begin()->second->getType(), results.size());
  for (auto it = results.begin(); it != results.end(); it++) {
    phi->addIncoming(it->second, it->first);
  }
  state.definitions[&*var] = phi;
  auto body_result = body->generate(state, read, header, error_fn, error_ctx);
  state->CreateStore(state->CreateAdd(state->CreateLoad(index),
                                      llvm::ConstantInt::get(type, 1)),
                     index);
  state->CreateCondBr(body_result,
                      all ? next_block : merge_block,
                      all ? merge_block : next_block);
  body_block = state->GetInsertBlock();

  state->SetInsertPoint(merge_block);
  auto final_result =
      state->CreatePHI(llvm::Type::getInt1Ty(state.module()->getContext()), 2);
  auto true_val = llvm::ConstantInt::getFalse(state.module()->getContext());
  auto false_val = llvm::ConstantInt::getTrue(state.module()->getContext());
  final_result->addIncoming(all ? false_val : true_val, next_block);
  final_result->addIncoming(all ? true_val : false_val, body_block);
  return final_result;
}
llvm::Value *bamql::LoopNode::generateIndex(bamql::GenerateState &state,
                                            llvm::Value *chromosome,
                                            llvm::Value *header,
                                            llvm::Value *error_fn,
                                            llvm::Value *error_ctx) {
  return llvm::ConstantInt::getTrue(state.module()->getContext());
}
bool bamql::LoopNode::usesIndex() { return false; }
bamql::ExprType bamql::LoopNode::type() { return bamql::BOOL; }
void bamql::LoopNode::writeDebug(GenerateState &state) {}
