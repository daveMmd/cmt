/*
 * nfheap.h
 *
 *  Created on: Jun 1, 2013
 *      Author: teng
 *      Memory management tool with 'boundary tag' algorithm and 'first/next fit' strategy
 */

//void Usage() {
//	char buf[10000];
//	NFHeap* h = (NFHeap*) buf;
//	NFHeap_init(h, sizeof(buf));
//	void* p = NFHeap_alloc(h, 1000);
//	NFHeap_free(h, p);
//	NFHeap_check(h, stdout, 1);
//}
#pragma once
#if 0
#	define CMTSCOPE
#	include <stdint.h>
#	include <stdbool.h>
#	include <stddef.h>
#	include <stdio.h>
#	include <string.h>
#else
#	include "common.h"
#endif

typedef struct _NFHeapStat {
	char* beg;
	char* brk;
	char* max;
	size_t allocated;
	size_t traversed;
} NFHeapStat;

typedef struct _NFHeap NFHeap;
CMTSCOPE uint32_t NFHeap_minSize();
CMTSCOPE void NFHeap_init(NFHeap* h, uint32_t size);
CMTSCOPE void* NFHeap_alloc(NFHeap* h, uint32_t size);
CMTSCOPE void NFHeap_free(NFHeap* h, void* p);
CMTSCOPE void* NFHeap_realloc(NFHeap* h, void* p, uint32_t size);
CMTSCOPE void NFHeap_check(NFHeap* h, FILE* f, bool verbose);
CMTSCOPE void NFHeap_stat(NFHeap* h, NFHeapStat* stat);
CMTSCOPE bool NFHeap_isEmpty(NFHeap* h);
CMTSCOPE bool NFHeap_isValidAddress(NFHeap* h, void* addr);
