#include <stdlib.h>
#include "barf-index.h"

#define hts_itr_destroy0(x) (((x) == NULL) ? NULL : ((x) = (hts_itr_destroy(x), NULL)))

struct barf_iterator {
	size_t ref_count;
	htsFile *file;
	BarfDestroy file_destructor;
	hts_idx_t *index;
	BarfDestroy index_destructor;
	BarfIteratorNext next;
	uint32_t state;
	hts_itr_t *iterator;
};

BarfIterator barf_iterator_new(htsFile *file, BarfDestroy file_destructor, hts_idx_t *index, BarfDestroy index_destructor, BarfIteratorNext next) {
	BarfIterator self;
	if (next == NULL)
		return NULL;
	if (index == NULL)
		return NULL;

	self = malloc(sizeof *self);
	if (self == NULL)
		return NULL;

	self->ref_count = 1;
	self->file = file;
	self->file_destructor = file_destructor;
	self->index = index;
	self->index_destructor = index_destructor;
	self->next = next;
	self->state = 0;
	self->iterator = NULL;
}

bool barf_iterator_next(BarfIterator self, bam1_t *read) {
	int tid;
	int begin;
	int end;

	while (true) {
		if (self->iterator == NULL && self->next(&self->state, &tid, &begin, &end)) {
			self->iterator = bam_itr_queryi(self->index, tid, begin, end);
		}

		if (self->iterator == NULL) {
			return false;
		}

		if (bam_itr_next(self->file, self->iterator, read) >= 0) {
			return true;
		}
		hts_itr_destroy0(self->iterator);
	}
}

BarfIterator barf_iterator_ref(BarfIterator self) {
	if (self != NULL) {
		__sync_fetch_and_add(&self->ref_count, 1);
	}
	return self;
}
void barf_iterator_unref(BarfIterator self) {
	if (self == NULL)
		return;
	if (__sync_fetch_and_sub(&self->ref_count, 1) == 0) {
		hts_itr_destroy0(self->iterator);
		if (self->index_destructor != NULL) {
			self->index_destructor(self->index);
		}
		if (self->file_destructor != NULL) {
			self->file_destructor(self->file);
		}
		free(self);
	}
}

