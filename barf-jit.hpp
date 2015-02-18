#pragma once
#include <barf.hpp>
#include <htslib/hts.h>
#include <htslib/sam.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>

// vim: set ts=2 sw=2 tw=0 :

namespace barf {

/**
 * The run-time type of a filter.
 */
typedef bool (*filter_function)(bam_hdr_t *, bam1_t *);

/**
 * The run-time type of an index checker.
 */
typedef bool (*index_function)(bam_hdr_t *, uint32_t);

/**
 * Call the JITer on a function and return it as the correct type.
 */
template <typename T>
T getNativeFunction(std::shared_ptr<llvm::ExecutionEngine> &engine,
										llvm::Function *function) {
	union {
		T func;
		void *ptr;
	} result;
	result.ptr = engine->getPointerToFunction(function);
	return result.func;
}

/**
 * Create a JIT.
 */
std::shared_ptr<llvm::ExecutionEngine> createEngine(llvm::Module *module);

class read_iterator {
public:
	read_iterator();
	virtual bool wantChromosome(std::shared_ptr<bam_hdr_t> &header,
															uint32_t tid) = 0;
	virtual void processRead(std::shared_ptr<bam_hdr_t> &header,
													 std::shared_ptr<bam1_t> &read) = 0;
	virtual void ingestHeader(std::shared_ptr<bam_hdr_t> &header) = 0;
	bool processFile(const char *file_name, bool binary, bool ignore_index);
};
class check_iterator : public read_iterator {
public:
	check_iterator(std::shared_ptr<llvm::ExecutionEngine> &engine,
								 llvm::Module *module,
								 std::shared_ptr<ast_node> &node,
								 std::string name);
	virtual bool wantChromosome(std::shared_ptr<bam_hdr_t> &header, uint32_t tid);
	virtual void processRead(std::shared_ptr<bam_hdr_t> &header,
													 std::shared_ptr<bam1_t> &read);
	virtual void ingestHeader(std::shared_ptr<bam_hdr_t> &header) = 0;
	virtual void readMatch(bool matches,
												 std::shared_ptr<bam_hdr_t> &header,
												 std::shared_ptr<bam1_t> &read) = 0;

private:
	barf::filter_function filter;
	barf::index_function index;
	std::shared_ptr<llvm::ExecutionEngine> engine;
};
}
