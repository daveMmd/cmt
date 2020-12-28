#pragma once

#ifndef CMT_DIM
#	define CMT_DIM 2
#endif

#ifndef CMT_BIT
#	define CMT_BIT 64
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#if 1
// API
#	include <unistd.h>
#	define USLEEP(x) usleep(x)
#	define ATOMIC_ASSIGN(x, y) __sync_lock_test_and_set(&x, y)
#else
// API
#	include "cvmx.h"
#	include "cvmx-atomic.h"
#	include "cvmx-access.h"
#	define USLEEP(x) cvmx_wait_usec(x)
#	define ATOMIC_ASSIGN(x, y) cvmx_atomic_swap64_nosync((uintptr_t*)&x, (uintptr_t)y)
#endif

///////////////////////////////////////////////////////////////////////////////
// Global Macros

//#define CMTDBG printf
#define CMTDBG(...) ((void)0);

//#define _ printf("%s, %d\n", __FILE__, __LINE__);
#define _ ;

#define CONTAINER(p, type, tag) ((type *)((char*)(p)-(char*)(&((type*)0)->tag)))

#if 0
#	include <execinfo.h>
#	define BACKTRACE() {\
		void* callstack[128];\
		int i, frames = backtrace(callstack, 128);\
		char** strs = backtrace_symbols(callstack, frames);\
		for (i = 0; i < frames; ++i) {\
			printf("%s\n", strs[i]);\
		}\
		free(strs);\
	}
#else
#	define BACKTRACE() ((void)0);
#endif

#ifdef CMTINLINE
#	define CMTSCOPE static
#	define CMTFWDCL static // Forward declaration
#else
#	define CMTSCOPE
#	define CMTFWDCL extern
#endif

#if CMT_BIT == 8
#	define CMT_DATA uint8_t
#	define CMT_DFMT "%02"PRIx8
#elif CMT_BIT == 16
#	define CMT_DATA uint16_t
#	define CMT_DFMT "%04"PRIx16
#elif CMT_BIT == 32
#	define CMT_DATA uint32_t
#	define CMT_DFMT "%08"PRIx32
#elif CMT_BIT == 64
#	define CMT_DATA uint64_t
#	define CMT_DFMT "%016"PRIx64
#endif

///////////////////////////////////////////////////////////////////////////////
// Global Inline Functions
#define TYMASK (sizeof(uintptr_t)-1)
static inline void* getptr(uintptr_t p, int type) {
	if ((p & TYMASK) == type) {
		return (void*) (p & ~TYMASK);
	}
	return NULL;
}
static inline uintptr_t makeptr(void* p, int type) {
	return (((uintptr_t) p & ~TYMASK) | (type & TYMASK));
}
static inline int ptrtype(uintptr_t p) {
	return p & TYMASK;
}
#undef TYMASK

///////////////////////////////////////////////////////////////////////////////
// Global Functions
CMTSCOPE void heap_init(void* addr, size_t size);
CMTSCOPE void heap_debug(size_t* usage);
CMTSCOPE void* MNEW(size_t n);
CMTSCOPE void MDEL(void* p);
CMTSCOPE void* OFFSET(void* p);
CMTSCOPE size_t nearest_prime(size_t n);
CMTSCOPE void interval_to_pc16(uint16_t low, uint16_t high, uint16_t* d,
		uint16_t* m);
CMTSCOPE void pc_to_interval16(uint16_t d, uint16_t m, uint16_t* low,
		uint16_t* high);
CMTSCOPE uint32_t bit_to_mask32(uint8_t bit);

///////////////////////////////////////////////////////////////////////////////
// Global Structures
typedef char Buf48[256];
typedef char Buf1024[1024];
typedef struct _Data Data;
typedef Data Mask;
typedef struct _Rule Rule;
typedef struct _CMTRuleWrapper CMTRuleWrapper;
typedef struct _CMTNode CMTNode;
typedef struct _CMTItem CMTItem;
typedef struct _CMT CMT;
typedef struct _Vector Vector;
typedef void (*OnErase)(void* p);
typedef Rule* (*OnFind)(void* par, Data* d, Rule* r);
typedef int (*OnCompare)(const void* p1, const void* p2);

///////////////////////////////////////////////////////////////////////////////
struct _Data {
	CMT_DATA at[CMT_DIM];
};

///////////////////////////////////////////////////////////////////////////////
CMTSCOPE char* Mask_serialize(Mask* p, char* buf);
static inline void Mask_init(Mask* p) {
	memset(p, 0xFF, sizeof(*p));
}
// m1 contains m2?
static inline bool Mask_contains(Mask* m1, Mask* m2) {
	size_t i;
	// If m1 contains m2, we have m1 & m2 = m2
	for (i = 0; i != CMT_DIM; ++i) {
		if ((m1->at[i] & m2->at[i]) != m2->at[i]) {
			return false;
		}
	}
	return true;
}
// m1 = m2?
static inline bool Mask_eq(Mask* m1, Mask* m2) {
	size_t i;
	for (i = 0; i != CMT_DIM; ++i) {
		if (m1->at[i] != m2->at[i]) {
			return false;
		}
	}
	return true;
}
static inline Mask Mask_and(Mask* m1, Mask* m2) {
	Mask m;
	size_t i;
	for (i = 0; i != CMT_DIM; ++i) {
		m.at[i] = m1->at[i] & m2->at[i];
	}
	return m;
}
///////////////////////////////////////////////////////////////////////////////
CMTSCOPE char* Data_serialize(Data* p, char* buf);
CMTSCOPE int Data_cmp(const Data* d1, const Data* d2);
static inline bool Data_eq(Data* d1, Data* d2) {
	size_t i;
	for (i = 0; i != CMT_DIM; ++i) {
		if (d1->at[i] != d2->at[i]) {
			return false;
		}
	}
	return true;
}
static inline Data Data_maskWith(Data* p, Mask* m) {
	Data x;
	size_t i;
	for (i = 0; i != CMT_DIM; ++i) {
		x.at[i] = p->at[i] & m->at[i];
	}
	return x;
}

///////////////////////////////////////////////////////////////////////////////
struct _Rule {
	Data d;
	Mask m;
	size_t id;
	enum {
		TY_PARENT = 1, TY_NEXT = 2,
	};
	uintptr_t parent_or_next; // Only the last item of the rule list stores parent pointer
};
CMTSCOPE void Rule_init(Rule* p);
CMTSCOPE void Rule_trim(Rule* p);
CMTSCOPE char* Rule_serialize(Rule* p, char* buf);
static inline Rule* Rule_next(Rule* p) {
	return (Rule*) getptr(p->parent_or_next, TY_NEXT);
}
static inline CMTItem* Rule_parent(Rule* p) {
	Rule* x = p;
	while (ptrtype(x->parent_or_next) == TY_NEXT) {
		x = Rule_next(x);
	}
	return (CMTItem*) getptr(x->parent_or_next, TY_PARENT);
}

///////////////////////////////////////////////////////////////////////////////
//CMTFWDCL bool dbg;
CMTFWDCL int mem_acc;
//#define MEMACC printf("%d\n", ++mem_acc);
#define MEMACC (++mem_acc);
//#define MEMACC

