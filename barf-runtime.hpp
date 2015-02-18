#pragma once
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
T getNativeFunction(std::shared_ptr<llvm::ExecutionEngine> engine,
										llvm::Function *function) {
	union {
		T func;
		void *ptr;
	} result;
	result.ptr = engine->getPointerToFunction(function);
	return result.func;
}
}
