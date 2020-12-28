#pragma once
#include "common.h"

///////////////////////////////////////////////////////////////////////////////
struct _Vector {
	uint32_t size;
	uint32_t capacity;
	void** at;
};
CMTSCOPE void Vector_init(Vector* p, uint32_t size);
CMTSCOPE void Vector_reset(Vector* p, OnErase cb);
CMTSCOPE void Vector_push(Vector* p, void* x);
CMTSCOPE void Vector_erase(Vector* p, uint32_t b, uint32_t e, OnErase cb);
CMTSCOPE void Vector_sort(Vector* p, OnCompare cmp);
CMTSCOPE void Vector_append(Vector* p, Vector* rhs);
CMTSCOPE void Vector_swap(Vector* v1, Vector* v2);
CMTSCOPE void Vector_resize(Vector* p, uint32_t size, OnErase cb);
CMTSCOPE void Vector_vacuum(Vector* p);
