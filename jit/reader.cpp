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

bamql::CompileIterator::CompileIterator(
    std::shared_ptr<CompiledPredicate> &predicate_)
    : predicate(predicate_) {}

bool bamql::CompileIterator::wantChromosome(std::shared_ptr<bam_hdr_t> &header,
                                            uint32_t tid) {
  return predicate->wantChromosome(
      header, tid, [&](const char *message) { this->handleError(message); });
}

void bamql::CompileIterator::processRead(std::shared_ptr<bam_hdr_t> &header,
                                         std::shared_ptr<bam1_t> &read) {
  readMatch(predicate->wantRead(
                header, read,
                [&](const char *message) { this->handleError(message); }),
            header, read);
}
