#include <iostream>
#include <set>
#include <sstream>
#include <utility>
#include <vector>
#include "barf.hpp"
#include "barf-jit.hpp"

/*
 * Each pair is a query and the names of the sequences from test.sam that
 * match.
 */
std::vector<std::pair<std::string, std::set<std::string>>> queries = {
  { "mapping_quality(0.5)", { "E", "F" } },
  { "before(10060)", { "A", "B", "C", "D" } },
  { "nt(10360, A)", { "E", "F" } },
  { "nt(10360, R)", { "E", "F" } },
  { "nt_exact(10360, R)", {} },
  { "paired?", { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J" } },
  { "mate_unmapped?", {} },
  { "read_group(C3BUK.1)", { "A", "J" } },
  { "read_group(C3BUK.1 )", { "A", "J" } },
  { "read_group( C3BUK.1 )", { "A", "J" } },
  { "chr(1)", { "A", "B", "C", "D", "E" } },
  { "chr(*2)", { "F", "G", "H", "I", "J" } },
  { "chr(1*)", { "A", "B", "C", "D", "E", "F", "G", "H", "J" } },
  { "mate_chr(1)", { "A", "B", "E", "G" } },
  { "read_group(C3BUK.1) then chr(2) else chr(12)", { "F", "G", "H" } },
  { "read_group(C3BUK.1) then chr(1) else chr(2)", { "A", "I" } },
  { "!chr(1)", { "F", "G", "H", "I", "J" } },
  { "chr(1*) | chr(*2)", { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J" } },
  { "chr(1*) & chr(*2)", { "F", "G", "H", "J" } }
};

class Checker : public barf::CheckIterator {
public:
  Checker(std::shared_ptr<llvm::ExecutionEngine> &engine,
          llvm::Module *module,
          std::shared_ptr<barf::AstNode> &node,
          std::string name,
          int index_)
      : barf::CheckIterator::CheckIterator(engine, module, node, name),
        correct(true), index(index_) {}
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
  bool isCorrect() { return correct; }

private:
  int index;
  bool correct;
};

int main(int argc, char *const *argv) {
  bool success = true;
  LLVMInitializeNativeTarget();
  auto module = new llvm::Module("barf", llvm::getGlobalContext());
  auto engine = barf::createEngine(module);
  if (!engine) {
    std::cerr << "Failed to initialise LLVM." << std::endl;
    return 1;
  }
  for (int index = 0; index < queries.size(); index++) {
    auto ast = barf::AstNode::parseWithLogging(queries[index].first,
                                               barf::getDefaultPredicates());
    if (!ast) {
      std::cerr << "Could not compile test: " << queries[index].first
                << std::endl;
      return 1;
    }
    std::stringstream name;
    name << "test" << index;
    Checker checker(engine, module, ast, name.str(), index);
    bool test_success =
        checker.processFile("test.sam", false, false) && checker.isCorrect();
    std::cerr << index << " " << (test_success ? "----" : "FAIL") << " "
              << queries[index].first << std::endl;
    success &= test_success;
  }
  return success ? 0 : 1;
}
