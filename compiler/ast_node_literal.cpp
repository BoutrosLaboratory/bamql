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

#include "ast_node_literal.hpp"
#include "bamql-compiler.hpp"

namespace bamql {

llvm::Value *make_bool(llvm::LLVMContext &context, bool value) {
  return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), value);
}

llvm::Value *make_char(llvm::LLVMContext &context, char value) {
  return llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), value);
}

llvm::Value *make_int(llvm::LLVMContext &context, int value) {
  return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), value);
}

llvm::Value *make_dbl(llvm::LLVMContext &context, double value) {
  return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), value);
}
} // namespace bamql
