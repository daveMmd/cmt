/*
 * nfheap.c
 *
 *  Created on: Jun 1, 2013
 *      Author: teng
 */
#include "nfheap.h"

#define NEXTFIT
#define ROUNDUP(size) (((((size) - 1) >> 3) + 1) << 3)
#define FITMAX 50

// Byte alignment: 8
///////////////////////////////////////////////////////////////////////////////
// NFHeapNodeTag
typedef uint32_t NFHeapNodeTag;
#define TAGNEW(size, is_free) ((size) | (is_free))
#define TAGGETSIZE(t) ((t) & ~0x7)
#define TAGISFREE(t) ((t) & 0x1)
#define TAGSETFREE(t) ((t) |= 0x1)
#define TAGSETNFREE(t) ((t) &= ~0x1)

///////////////////////////////////////////////////////////////////////////////
// NFHeapNode
typedef struct _NFHeapNode NFHeapNode;
struct _NFHeapNode {
	// Put prevFooter here to make 8 byte alignment
	NFHeapNodeTag f_prev;
	NFHeapNodeTag h;
	union {
		struct {
			NFHeapNode* prev;
			NFHeapNode* next;
		};
		char d[2 * sizeof(NFHeapNode*)];
	};
};
static inline NFHeapNode* NFHeapNode_fromData(char* d) {
	return (NFHeapNode*) (d - 2 * sizeof(NFHeapNodeTag));
}
static inline NFHeapNode* NFHeapNode_getNextNode(NFHeapNode* n) {
	return (NFHeapNode*) (n->d + TAGGETSIZE(n->h));
}
static inline NFHeapNodeTag* NFHeapNode_getFooter(NFHeapNode* n) {
	return &NFHeapNode_getNextNode(n)->f_prev;
}
static inline NFHeapNodeTag* NFHeapNode_getPrevFooter(NFHeapNode* n) {
	return &n->f_prev;
}
static inline NFHeapNode* NFHeapNode_getPrevNode(NFHeapNode* n) {
	NFHeapNodeTag* pf = NFHeapNode_getPrevFooter(n);
	return (NFHeapNode*) (((char*) pf) - TAGGETSIZE(*pf)
			- 2 * sizeof(NFHeapNodeTag));
}

static inline void NFHeapNode_take(NFHeapNode* n) {
	n->prev->next = n->next;
	n->next->prev = n->prev;
}
static inline void NFHeapNode_replace(NFHeapNode* n, NFHeapNode* other) {
	n->prev->next = other;
	n->next->prev = other;
	other->prev = n->prev;
	other->next = n->next;
}

///////////////////////////////////////////////////////////////////////////////
// NFHeap
struct _NFHeap {
	char* max;
	char* brk;
#ifdef NEXTFIT
	NFHeapNode* rover;
#endif
	NFHeapNode prol;
	NFHeapNode epil;
	char d[1];
};
CMTSCOPE NFHeapNode* NFHeap_findFit(NFHeap* h, uint32_t size, size_t fitmax) {
	NFHeapNode* n;
	size_t count = 0;
#ifdef NEXTFIT
	n = h->rover;
	// Search to epil

	while (n->next) {
		if (count++ == fitmax) {
			goto NOTFOUND;
		}
		if (TAGGETSIZE(n->h) >= size) {
			h->rover = n;
			goto FOUND;
		}
		n = n->next;
	}
	// Not found, search from prol to rover
	n = h->prol.next;
	while (n != h->rover) {
		if (count++ == fitmax) {
			goto NOTFOUND;
		}
		if (TAGGETSIZE(n->h) >= size) {
			h->rover = n;
			goto FOUND;
		}
		n = n->next;
	}
#else
	// First fit
	n = h->prol.next;
	while (n->next) {
		if (count++ == fitmax) {
			goto NOTFOUND;
		}
		if (TAGGETSIZE(n->h) >= size) {
			goto FOUND;
		}
		n = n->next;
	}
#endif

	NOTFOUND: {
		return NULL;
	}

	FOUND: {
		return n;
	}
}
CMTSCOPE NFHeapNode* NFHeap_sbrk(NFHeap* h, uint32_t size) {
	char* new_brk = h->brk + 2 * sizeof(NFHeapNodeTag) + size;
	if (new_brk + sizeof(NFHeapNodeTag) > h->max) {
		// Size not enough (Must reserve a footer for node)
		return NULL;
	}
	NFHeapNode* n = (NFHeapNode*) h->brk;
	n->h = TAGNEW(size, 0);
	*NFHeapNode_getFooter(n) = n->h;
	h->brk = new_brk;
	return n;
}
// Before |H|n      |F|
// After  |H|e|F|H|r|F|
CMTSCOPE NFHeapNode* NFHeap_extract(NFHeap* h, NFHeapNode* n, uint32_t size) {
	uint32_t nsize = TAGGETSIZE(n->h);
	uint32_t esize = size + 2 * sizeof(NFHeapNodeTag);
	NFHeapNode* e = n;
	if (esize + sizeof(n->d) <= nsize) {
		// Do extraction
		e->h = TAGNEW(size, 0);

		NFHeapNode* r = NFHeapNode_getNextNode(e);
		r->h = TAGNEW(nsize - esize, 1);

		NFHeapNode_replace(e, r);
		*NFHeapNode_getFooter(e) = e->h;
		*NFHeapNode_getFooter(r) = r->h;

#ifdef NEXTFIT
		h->rover = r;
#endif

	} else {

#ifdef NEXTFIT
		h->rover = e->next;
#endif

		NFHeapNode_take(e);
		TAGSETNFREE(e->h);
		*NFHeapNode_getFooter(e) = e->h;
	}
	return e;
}
CMTSCOPE uint32_t NFHeap_minSize() {
	NFHeap* h = NULL;
	return (size_t) &h->d - (size_t) h;
}
CMTSCOPE void NFHeap_init(NFHeap* h, uint32_t size) {
	h->max = ((char*) h) + size;
	h->brk = h->d;

	NFHeapNode* n;

	NFHeapNode* prol = &h->prol;
	prol->h = TAGNEW(sizeof(n->d), 0);

	NFHeapNode* epil = &h->epil;
	epil->h = TAGNEW(sizeof(n->d), 0);

	prol->prev = NULL;
	prol->next = epil;

	epil->prev = prol;
	epil->next = NULL;

#ifdef NEXTFIT
	h->rover = h->prol.next;
#endif
}
CMTSCOPE void* NFHeap_alloc(NFHeap* h, uint32_t size) {
	NFHeapNode* n;

	if (size <= sizeof(n->d)) {
		size = sizeof(n->d);
	} else {
		// Round up to 8 bytes
		size = ROUNDUP(size);
	}
	n = NFHeap_findFit(h, size, FITMAX);
	if (n) {
		n = NFHeap_extract(h, n, size);
	} else {
		n = NFHeap_sbrk(h, size);
		if (!n) {
			// NFHeap is full
			n = NFHeap_findFit(h, size, SIZE_MAX);
			if (n) {
				n = NFHeap_extract(h, n, size);
			} else {
				printf("NFHeap_alloc: Heap is full\n");
				NFHeap_check(h, stdout, 0);
				return NULL;
			}
		}
	}
	return n->d;
}

CMTSCOPE void NFHeap_free(NFHeap* h, void* p) {
	if (!p) {
		return;
	}

	NFHeapNode* n = NFHeapNode_fromData((char*) p);
	n->prev = NULL;
	n->next = NULL;

	// Make sure n has neighbored node
	NFHeapNode* prev = NULL;
	if (((char*) n) > h->d) {
		prev = NFHeapNode_getPrevNode(n);
		if (!TAGISFREE(prev->h)) {
			prev = NULL;
		}
	}

	NFHeapNode* next = NFHeapNode_getNextNode(n);
	if ((char*) next >= h->brk || !TAGISFREE(next->h)) {
		next = NULL;
	}

#ifdef NEXTFIT
	// If 'next' is to be combined, never point rover to it
	if (next && h->rover == next) {
		h->rover = next->next;
	}
#endif
	if (prev) {
		if (next) {
			// Before |H|prev|F|H|n|F|H|next|F|
			// After  |H|prev               |F|
			uint32_t asize = TAGGETSIZE(prev->h) + TAGGETSIZE(n->h)
					+ TAGGETSIZE(next->h) + 4 * sizeof(NFHeapNodeTag);
			NFHeapNode_take(next);
			prev->h = TAGNEW(asize, 1);
			*NFHeapNode_getFooter(prev) = prev->h;
		} else {
			// Before |H|prev|F|H|n|F|
			// After  |H|prev      |F|
			uint32_t asize = TAGGETSIZE(prev->h) + TAGGETSIZE(n->h)
					+ 2 * sizeof(NFHeapNodeTag);
			prev->h = TAGNEW(asize, 1);
			*NFHeapNode_getFooter(prev) = prev->h;
		}
	} else {
		if (next) {
			// Before |H|n|F|H|next|F|
			// After  |H|n         |F|
			uint32_t asize = TAGGETSIZE(n->h) + TAGGETSIZE(next->h)
					+ 2 * sizeof(NFHeapNodeTag);
			// Replace 'next' with 'n'
			NFHeapNode_replace(next, n);
			n->h = TAGNEW(asize, 1);
			*NFHeapNode_getFooter(n) = n->h;
		} else {
			// Add node to tail
			n->prev = h->epil.prev;
			n->next = &h->epil;
			h->epil.prev->next = n;
			h->epil.prev = n;
			TAGSETFREE(n->h);
			*NFHeapNode_getFooter(n) = n->h;
		}
	}
}
CMTSCOPE void* NFHeap_realloc(NFHeap* h, void* p, uint32_t size) {
	uint32_t old_size = 0;
	if (p) {
		NFHeapNode* n = NFHeapNode_fromData((char*) p);
		old_size = TAGGETSIZE(n->h);
		if (size <= old_size) {
			return n->d;
		}
	}
	void* new_ptr = NFHeap_alloc(h, size);
	if (new_ptr && p) {
		memcpy(new_ptr, p, old_size);
		NFHeap_free(h, p);
	}
	return new_ptr;
}
CMTSCOPE void NFHeap_check(NFHeap* h, FILE* f, bool verbose) {
	NFHeapNode* n = (NFHeapNode*) h->d;
	if (verbose) {
		fprintf(f, "NFHeap_check begin\n");
	}
	size_t a = 0;
	while (((char*) n) < h->brk) {
		NFHeapNodeTag ht = n->h;
		NFHeapNodeTag ft = *NFHeapNode_getFooter(n);
		if (verbose) {
			fprintf(f, "%p, H:{size:%u, is_free:%d}, F:{size:%u, is_free:%d}",
					n, TAGGETSIZE(ht), TAGISFREE(ht), TAGGETSIZE(ft),
					TAGISFREE(ft));
			if (TAGISFREE(ht)) {
				fprintf(f, ", prev:%p, next:%p", n->prev, n->next);
			} else {
				a += TAGGETSIZE(ht);
			}
			fprintf(f, "\n");
		} else {
			if (!TAGISFREE(ht)) {
				a += TAGGETSIZE(ht);
			}
		}
		n = NFHeapNode_getNextNode(n);
	}
	size_t t = h->brk - h->d;
	fprintf(f,
			"NFHeap_check: beg:%p, brk:%p, max:%p, allocated:%u, traversed:%u\n",
			h->d, h->brk, h->max, a, t);
	if (verbose) {
		fprintf(f, "NFHeap_check end\n");
	}
}
CMTSCOPE void NFHeap_stat(NFHeap* h, NFHeapStat* stat) {
	NFHeapNode* n = (NFHeapNode*) h->d;
	size_t a = 0;
	while (((char*) n) < h->brk) {
		NFHeapNodeTag ht = n->h;
		NFHeapNodeTag ft = *NFHeapNode_getFooter(n);

		if (!TAGISFREE(ht)) {
			a += TAGGETSIZE(ht);
		}
		n = NFHeapNode_getNextNode(n);
	}
	size_t t = h->brk - h->d;
	stat->beg = h->d;
	stat->brk = h->brk;
	stat->max = h->max;
	stat->allocated = a;
	stat->traversed = t;
}

CMTSCOPE bool NFHeap_isEmpty(NFHeap* h) {
	uint32_t used = h->brk - h->d;
	if (used == 0) {
		// Heap is empty
		return true;
	}

	NFHeapNode* n = h->prol.next;
	if (n == &h->epil) {
		// Used, but no free nodes found
		return false;
	}

	// Only has one merged free node, and has the whole used size
	if (TAGGETSIZE(n->h) + 2 * sizeof(NFHeapNodeTag) == used) {
		return true;
	}

	return false;
}

CMTSCOPE bool NFHeap_isValidAddress(NFHeap* h, void* addr) {
	return (char*) addr >= h->d && (char*) addr <= h->brk;
}
