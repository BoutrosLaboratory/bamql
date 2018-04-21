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

#include "bamql-jit.hpp"
#include <sstream>

static void error_wrapper(const char *message, void *context) {
  ((bamql::CompileIterator *)context)->handleError(message);
}

bamql::CompileIterator::CompileIterator(
    std::shared_ptr<llvm::ExecutionEngine> &e,
    std::shared_ptr<Generator> &generator,
    std::shared_ptr<AstNode> &node,
    const std::string &name)
    : engine(e), filter_func(node->createFilterFunction(generator, name)) {
  // Compile the query into native functions. We must hold a reference to the
  // execution engine as long as we intend for these pointers to be valid.
  std::stringstream index_function_name;
  index_function_name << name << "_index";
  index_func = node->createIndexFunction(generator, index_function_name.str());
}

void bamql::CompileIterator::prepareExecution() {
  filter = getNativeFunction<FilterFunction>(engine, filter_func);
  index = getNativeFunction<IndexFunction>(engine, index_func);
}

bool bamql::CompileIterator::wantChromosome(std::shared_ptr<bam_hdr_t> &header,
                                            uint32_t tid) {
  return index(header.get(), tid, error_wrapper, this);
}

void bamql::CompileIterator::processRead(std::shared_ptr<bam_hdr_t> &header,
                                         std::shared_ptr<bam1_t> &read) {
  readMatch(filter(header.get(), read.get(), error_wrapper, this), header,
            read);
}
