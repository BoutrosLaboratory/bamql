#ifndef BARF_INDEX_H
#define BARF_INDEX_H
#include <stdbool.h>
#include <htslib/sam.h>

typedef struct barf_iterator *BarfIterator;
/**
 * Get the next region of the input file to be searched.
 *
 * This signature is really meant to be used by the generated code.
 *
 * The function must suggest the next region of interest to extract from the
 * index. Reads from that region will then be filtered and then this function
 * called again to get the subsequent region.
 *
 * @state:(inout): The current state. This is opaque to the caller, but it will be initialised to zero.
 * @tid:(inout): The chromosome of interest. Initially, this will be zero.
 * @begin:(out): The 1-base start of the region to scan. This must be set if function returns true.
 * @end:(out): The 1-base end of the region to scan. This must be set if function returns true.
 * Return: true if there is a region of interest, false if there are no more regions.
 */
typedef bool (*BarfIteratorNext)(uint32_t *state, uint32_t *tid, uint32_t *begin, uint32_t *end);

typedef void (*BarfDestroy)(void *object);

/**
 * Create a new smart iterator for a query.
 *
 * @file:(transfer floating): The SAM/BAM file to be searched.
 * @file_destructor:(destroy file)(nullable): Called when the file is no longer needed.
 * @index:(transfer floating): The corresponding index for the SAM/BAM file.
 * @index_destructor:(destroy index)(nullable): Called when the index is no longer needed.
 * @next: A function provided by the generated code that requests the next region.
 * Returns: An iterator appropriate for the query.
 */
BarfIterator barf_iterator_new(htsFile *file, BarfDestroy file_destructor, hts_idx_t *index, BarfDestroy index_destructor, BarfIteratorNext next);
/**
 * Get the next read to consider.
 *
 * @read: The holder for the SAM/BAM data.
 * Returns: True if a read was available, false otherwise.
 */
bool barf_iterator_next(BarfIterator self, bam1_t *read);
/**
 * Increase the reference count on the iterator.
 */
BarfIterator barf_iterator_ref(BarfIterator self);
/**
 * Decrease the reference count on the iterator.
 *
 * @self:(tranfer full): The instance to be released.
 */
void barf_iterator_unref(BarfIterator self);

#endif
