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
#include "bamql-jit.hpp"
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

/*
 * Each pair is a query and the names of the sequences from test.sam that
 * match.
 */
std::vector<std::pair<std::string, std::set<std::string>>> queries = {
  { "true", { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J" } },
  { "mapping_quality(0.5)", { "E", "F" } },
  { "before(10060)", { "A", "B", "C", "D" } },
  { "let x = before(10060), y = mapping_quality(0.5) in x | y",
    { "A", "B", "C", "D", "E", "F" } },
  { "nt(10360, C)", { "E", "F" } },
  { "nt(10360, Y)", { "E", "F" } },
  { "nt_exact(10360, C)", { "E", "F" } },
  { "nt_exact(10360, Y)", {} },
  { "paired?", { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J" } },
  { "raw_flag(99)", { "F", "I", "J" } },
  { "flags \\ 99", { "F", "I", "J" } },
  { "mate_unmapped?", {} },
  { "split_pair?", { "C", "D", "G" } },
  { "read_group ~ /C3BUK.1/", { "A", "J" } },
  { "aux_int(NM) == 1", { "B", "E", "F" } },
  { "aux_str(MD) ~ /51/", { "D" } },
  { "aux_int(XC) == 'b", { "G" } },
  { "aux_dbl(XB) < 3.15", { "C", "D" } },
  { "aux_dbl(XB) == 2.0", { "C", "D" } },
  { "chr(1)", { "A", "B", "C", "D", "E" } },
  { "chr(*2)", { "F", "G", "H", "I", "J" } },
  { "chr(1*)", { "A", "B", "C", "D", "E", "F", "G", "H", "J" } },
  { "mate_chr(1)", { "A", "B", "E", "G" } },
  { "header ~ /A/", { "A" } },
  { "read_group ~ /C3BUK.1/ then chr(2) else chr(12)", { "F", "G", "H" } },
  { "read_group ~ /C3BUK.1/ then chr(1) else chr(2)", { "A", "I" } },
  { "!chr(1)", { "F", "G", "H", "I", "J" } },
  { "chr(1*) | chr(*2)", { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J" } },
  { "chr(1*) & chr(*2)", { "F", "G", "H", "J" } },
  { "chr(1*) ^ chr(*2)", { "A", "B", "C", "D", "E", "I" } },
  { "chr(1) -> read_group : C3BUK.*", { "A", "E", "F", "G", "H", "I", "J" } },
  { "end < begin", {} },
  { "!(any x = 3, 4 in x == 3)", {} },
  { "all x = 3, 4 in x == 3", {} },
  { "any x = 3, 4 in x < 3", {} },
  { "!(all x = 3, 4 in x > 1)", {} },
  { "bind read_group using /C3BUK(?<x_d>\\.\\d)/ in x_d < 0.15", { "A", "J" } },
  { "bind read_group using /C3BUK\\.(?<x_i>\\d)/ in x_i == 1", { "A", "J" } },
  { "bind header using /(?<x_c>.)/ in x_c == 'A", { "A" } },
  { "header ~ /a/i", { "A" } },
  { "max(3,4,5) == 5", { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J" } },
  { "min (3.1 , 4.0 , 5.2 ) < 3.5",
    { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J" } },
  { "min(read_group, header) == header", { "A", "B", "C" } },
  { "insert_size == 49", { "H", "I" } },
  { "mate_begin == 11439", { "J" } },
};

class Checker final : public bamql::CompileIterator {
public:
  Checker(std::shared_ptr<bamql::CompiledPredicate> predicate, int index_)
      : bamql::CompileIterator::CompileIterator(predicate), correct(true),
        index(index_) {}
  void ingestHeader(std::shared_ptr<bam_hdr_t> &header) {}
  void readMatch(bool matches,
                 std::shared_ptr<bam_hdr_t> &header,
                 std::shared_ptr<bam1_t> &read) {
    if (matches !=
        (queries[index].second.count(std::string(bam_get_qname(read))) == 1)) {
      std::cerr << queries[index].first << " is " << (matches ? "" : "not ")
                << "matching " << bam_get_qname(read) << " and that's wrong."
                << std::endl;
      correct = false;
    }
  }
  void handleError(const char *message) {}
  bool isCorrect() { return correct; }

private:
  bool correct;
  int index;
};

int main(int argc, char *const *argv) {
  bool success = true;
  auto jit = bamql::JIT::create();

  auto predicates = bamql::getDefaultPredicates();
  std::vector<Checker> checkers;
  for (size_t index = 0; index < queries.size(); index++) {
    auto ast =
        bamql::AstNode::parseWithLogging(queries[index].first, predicates);

    if (!ast) {
      std::cerr << "Could not compile test: " << queries[index].first
                << std::endl;
      return 1;
    }
    std::stringstream name;
    name << "test" << index;
    checkers.emplace_back(bamql::JIT::compile(jit, ast, name.str()), index);
  }

  for (size_t index = 0; index < queries.size(); index++) {
    bool test_success =
        checkers[index].processFile("test/test.sam", false, false) &&
        checkers[index].isCorrect();
    std::cerr << std::setw(2) << index << " "
              << (test_success ? "----" : "FAIL") << " " << queries[index].first
              << std::endl;
    success &= test_success;
  }
  return success ? 0 : 1;
}
