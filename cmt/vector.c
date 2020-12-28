#include "vector.h"

#define CAPMIN 1

CMTSCOPE void Vector_recap(Vector* p, uint32_t capacity) {
	if (capacity < CAPMIN) {
		capacity = CAPMIN;
	}
	if (capacity <= p->capacity) {
		return;
	}
	//printf("recap: %zu->%zu\n", p->capacity, capacity);
	Vector v;
	v.capacity = capacity;
	v.at = (void**) MNEW(sizeof(*v.at) * v.capacity);
	memset(v.at, 0, sizeof(*v.at) * v.capacity);
	if (p->at) {
		memcpy(v.at, p->at, sizeof(*p->at) * p->size);
	}
	v.size = p->size;
	Vector_reset(p, NULL);
	*p = v;
}

///////////////////////////////////////////////////////////////////////////////
CMTSCOPE void Vector_init(Vector* p, uint32_t size) {
	memset(p, 0, sizeof(*p));
	if (size) {
		Vector_recap(p, size);
		p->size = size;
	}
}
CMTSCOPE void Vector_reset(Vector* p, OnErase cb) {
	uint32_t i;
	if (cb) {
		for (i = 0; i < p->size; ++i) {
			if (p->at[i]) {
				cb(p->at[i]);
			}
		}
	}
	MDEL(p->at);
	Vector_init(p, 0);
}
CMTSCOPE void Vector_push(Vector* p, void* x) {
	if (p->size == p->capacity) {
		Vector_recap(p, 2 * p->capacity);
	}
	p->at[p->size++] = x;
}
CMTSCOPE void Vector_erase(Vector* p, uint32_t b, uint32_t e, OnErase cb) {
	uint32_t i, rsize = p->size - e;
	if (cb) {
		for (i = b; i < e; ++i) {
			if (p->at[i]) {
				cb(p->at[i]);
			}
		}
	}
	if (rsize) {
		memmove(&p->at[b], &p->at[e], sizeof(*p->at) * rsize);
	}
	p->size -= (e - b);
}
CMTSCOPE void Vector_sort(Vector* p, OnCompare cmp) {
	qsort(p->at, p->size, sizeof(*p->at), cmp);
}
CMTSCOPE void Vector_append(Vector* p, Vector* rhs) {
	if (!rhs->size) {
		return;
	}
	uint32_t n = p->size + rhs->size;
	if (n > p->capacity) {
		Vector_recap(p, 2 * n);
	}
	memcpy(&p->at[p->size], rhs->at, sizeof(*rhs->at) * rhs->size);
	p->size = n;
}
CMTSCOPE void Vector_swap(Vector* v1, Vector* v2) {
	Vector temp;
	temp = *v1;
	*v1 = *v2;
	*v2 = temp;
}
CMTSCOPE void Vector_resize(Vector* p, uint32_t size, OnErase cb) {
	if (size > p->capacity) {
		Vector_recap(p, 2 * size);
	}
	if (size < p->size) {
		Vector_erase(p, size, p->size, cb);
	} else {
		p->size = size;
	}
}
CMTSCOPE void Vector_vacuum(Vector* p) {
	if (p->size == p->capacity) {
		return;
	}
	Vector temp;
	Vector_init(&temp, p->size);
	if (p->size) {
		memcpy(temp.at, p->at, sizeof(*p->at) * p->size);
	}
	Vector_swap(p, &temp);
	Vector_reset(&temp, NULL);
}
