#include "common.h"
#include "nfheap.h"

static NFHeap* g_common_heap = NULL;
#define h g_common_heap
CMTSCOPE void heap_init(void* addr, size_t size) {
	h = (NFHeap*) addr;
	if (size) {
		NFHeap_init(h, size);
	}
}
CMTSCOPE void heap_debug(size_t* usage) {
	if (usage) {
		NFHeapStat stat;
		NFHeap_stat(h, &stat);
		*usage = stat.allocated;
	} else {
		NFHeap_check(h, stdout, 0);
	}
}
CMTSCOPE void* MNEW(size_t n) {
//	return malloc(n);
	void * p = NFHeap_alloc(h, n);
	return p;
}

CMTSCOPE void MDEL(void* p) {
//	return free(p);
	NFHeap_free(h, p);
}
CMTSCOPE void* OFFSET(void* p) {
	return (void*) ((void*) p - (void*) h);
}
#undef h
CMTSCOPE size_t nearest_prime(size_t n) {
//	size_t orig = n;
	for (n |= 1; n != 1; n -= 2) {
		size_t p;
		size_t q = 1;
		bool is_prime = true;
		for (p = n - 2; p > q; p -= 2) {
			q = n / p;
			if (n == p * q) {
				is_prime = false;
				break;
			}
		}
		if (is_prime) {
			break;
		}
	}
//	printf("%zu, %zu\n", orig, n);
	return n;
}
CMTSCOPE void interval_to_pc16(uint16_t low, uint16_t high, uint16_t* d,
		uint16_t* m) {
	int i;
	uint16_t and = ~(low ^ high);
	uint16_t value = 0;
	for (i = 15; i >= 0; --i) {
		// Highest bit first
		uint16_t flag = (1 << i);
		if (and & flag) {
			value = (value | (flag & low));
		} else {
			break;
		}
	}
	++i;
	*d = value;
	*m = (i == 16) ? 0 : -(1 << i);
}
CMTSCOPE void pc_to_interval16(uint16_t d, uint16_t m, uint16_t* low,
		uint16_t* high) {
	*low = d;
	*high = d + ~m;
}
CMTSCOPE uint32_t bit_to_mask32(uint8_t bit) {
	return bit ? -(1 << (32 - bit)) : 0;
}
///////////////////////////////////////////////////////////////////////////////
CMTSCOPE char* Mask_serialize(Mask* p, char* buf) {
	buf[0] = 0;
	sprintf(buf, "["CMT_DFMT, p->at[0]);
	Buf48 temp;
	size_t i;
	for (i = 1; i < CMT_DIM; ++i) {
		sprintf(temp, "/"CMT_DFMT, p->at[i]);
		strcat(buf, temp);
	}
	strcat(buf, "]");
	return buf;
}

/////////////////////////////////////////////////////////////////////////////
CMTSCOPE char* Data_serialize(Data* p, char* buf) {
	buf[0] = 0;
	sprintf(buf, "["CMT_DFMT, p->at[0]);
	Buf48 temp;
	size_t i;
	for (i = 1; i != CMT_DIM; ++i) {
		sprintf(temp, "/"CMT_DFMT, p->at[i]);
		strcat(buf, temp);
	}
	strcat(buf, "]");
	return buf;
}

CMTSCOPE int Data_cmp(const Data* d1, const Data* d2) {
	size_t i;
	for (i = 0; i != CMT_DIM; ++i) {
		if (d1->at[i] < d2->at[i]) {
			return -1; // d1 < d2
		} else if (d1->at[i] > d2->at[i]) {
			return 1; // d1 > d2
		}
	}
	return 0; // equal
}

///////////////////////////////////////////////////////////////////////////////
CMTSCOPE void Rule_init(Rule* p) {
	memset(p, 0, sizeof(*p));
}
CMTSCOPE void Rule_trim(Rule* p) {
	p->d = Data_maskWith(&p->d, &p->m);
}
CMTSCOPE char* Rule_serialize(Rule* p, char* buf) {
	buf[0] = 0;
	Buf48 b1, b2;
	sprintf(buf,
			"{id:%u, d:%s, m:%s}", p->id, Data_serialize(&p->d, b1), Mask_serialize(&p->m, b2));
	return buf;
}

///////////////////////////////////////////////////////////////////////////////
//CMTSCOPE bool dbg = 0;
CMTSCOPE int mem_acc = 0;
