#include "cmt.h"

typedef struct _CMTDebugInfo {
	char tab[100];
	size_t node_count;
	size_t rule_count;
	size_t subnode_count;
	int depth;
    uint32_t overall_depth;
    uint32_t sub_depth;
    uint32_t item_count;
} CMTDebugInfo;

static inline uint64_t hash(Data* d) {
	size_t i;
	uint64_t h = d->at[0];
	for (i = 1; i != CMT_DIM; ++i) {
		h += d->at[i];
	}
	return h;
}
static inline size_t hash1(uint64_t h, size_t n) {
	return h % n;
}
static inline size_t hash2(uint64_t h, size_t n) {
	return h % (n - 1) + 1;
}
static inline size_t get_probe(size_t h1, size_t h2, size_t nprobe, size_t n) {
	return (h1 + h2 * nprobe) % n;
}

static inline CMTNode* CMTNode_sub(CMTNode* p) {
	return (CMTNode*) getptr(p->sub_or_ib, TY_SUB);
}
static inline CMTItem* CMTNode_IB(CMTNode* p) {
	return (CMTItem*) getptr(p->sub_or_ib, TY_IB);
}
static inline void CMTNode_newIB(CMTNode* p) {
	CMTItem* ib = CMTItem_new(NULL, p);
	// TODO: ATOM
	ATOMIC_ASSIGN(p->sub_or_ib, makeptr(ib, TY_IB));
}

CMTSCOPE void CMTNode_debug(CMTNode* p, FILE* f, CMTDebugInfo* info, int depth) {
	info->depth = info->depth > depth ? info->depth:depth;
	size_t i, n = p->items.size;
	info->item_count += n;
	Buf1024 b1;
	//fprintf(f, "%s{count:%u, mask:%s, addr:%p, parent:%p}\n", info->tab,
	//		p->rule_count, Mask_serialize(&p->m, b1), p, p->parent);
	++info->node_count;
	for (i = 0; i != n; ++i) {
		CMTItem* item = p->items.at[i];
		if (!item) {
			continue;
		}
		//fprintf(f, "%s%u:%s\n", info->tab, i, CMTItem_serialize(item, b1));
		Rule* r = item->r;
		while (r) {
            info->overall_depth += depth;
			//fprintf(f, "%s  %s\n", info->tab, Rule_serialize(r, b1));
			++info->rule_count;
			r = Rule_next(r);
		}

		if (item->child) {
			size_t l = strlen(info->tab);
			info->tab[l] = '\t';
			CMTNode_debug(item->child, f, info, depth+1);
			info->tab[l] = '\0';
		}
	}

	switch (ptrtype(p->sub_or_ib)) {
	case TY_IB: {
		CMTItem* ib = CMTNode_IB(p);
		//fprintf(f, "%sIB:%s{\n", info->tab, CMTItem_serialize(ib, b1));
		Rule* r = ib->r;
		size_t ib_size = 0;
		while (r) {
			//fprintf(f, "%s%s\n", info->tab, Rule_serialize(r, b1));
			++info->rule_count;
			++ib_size;
			r = Rule_next(r);
		}
		//fprintf(f, "%s}, size:%u\n", info->tab, ib_size);
		break;
	}
	case TY_SUB: {
        info->sub_depth += depth;
		CMTNode* sub = CMTNode_sub(p);
		++info->subnode_count;
		//fprintf(f, "%sSUB:{size:%u}\n", info->tab, sub->items.size);
		CMTNode_debug(sub, f, info, depth);//sub-node可视作depth+1？
		break;
	}
	}
}
CMTSCOPE bool CMTNode_verify(CMTNode* p) {
	size_t i, n = p->items.size;
	size_t rule_count = 0;
	for (i = 0; i != n; ++i) {
		CMTItem* item = p->items.at[i];
		if (!item) {
			continue;
		}

		Rule* r = item->r;
		while (r) {
			++rule_count;
			r = Rule_next(r);
		}

		if (item->child) {
			CMTNode* x = item->child;
			while (x) {
				if (x->parent != item) {
					printf("Child parent mismatch, child:%p, parent:%p!\n", x,
							item);
					goto ABNORMAL;
				}

				rule_count += x->rule_count;
				x = CMTNode_sub(x);
			}
			CMTNode_verify(item->child);
		}
	}

	switch (ptrtype(p->sub_or_ib)) {
	case TY_IB: {
		CMTItem* ib = CMTNode_IB(p);
		if (ib->parent != p) {
			printf("IB parent mismatch, child:%p, parent:%p!\n", ib, p);
			goto ABNORMAL;
		}
		Rule* r = ib->r;
		size_t ib_size = 0;
		while (r) {
			++rule_count;
			r = Rule_next(r);
		}
		break;
	}
	case TY_SUB: {
		CMTNode_verify(CMTNode_sub(p));
		break;
	}
	}

	if (p->rule_count != rule_count) {
		printf("Rule count mismatch: p->rule_count:%u, rule_count:%u!\n",
				p->rule_count, rule_count);
		goto ABNORMAL;
	}

	return true;

	ABNORMAL: {
		CMTDebugInfo info = { 0 };
		CMTNode_debug(p, stdout, &info, 0);
		BACKTRACE();
		exit(0);
		return false;
	}
}
CMTSCOPE void CMTNode_addItem(CMTNode* p, CMTItem* x) {
	p->m = Mask_and(&p->m, &x->r->m);
	Vector_push(&p->items, x);
	x->parent = p;
	++p->rule_count;
}

CMTSCOPE void CMTNode_cut(CMTNode* p) {
	size_t i;

	CMTNode* sub = CMTNode_new(p->parent);
	//dave add
	sub->sub_last = p;

	Mask min_mask;
	Mask_init(&min_mask);

	size_t size = p->items.size;
	for (i = 0; i != size; ++i) {
		CMTItem* x = p->items.at[i];
		Mask new_mask = Mask_and(&min_mask, &x->r->m);
		if (Mask_eq(&new_mask, &p->m)) {
			CMTNode_addItem(sub, x);
			--p->rule_count;
			size_t last = size - 1;
			if (i != last) {
				// Erase 'i' by replacing with 'last'
				p->items.at[i] = p->items.at[last];
				--size;
				--i;
			} else {
				--size;
				break;
			}
		} else {
			min_mask = new_mask;
		}
	}
	Vector_erase(&p->items, size, p->items.size, NULL);
	p->m = min_mask;
	p->sub_or_ib = makeptr(sub, TY_SUB);
}

CMTSCOPE void CMTNode_rehash(CMTNode* p, Vector* tab, CMTNode* new_parent) {
	size_t i, j, h1, h2, n;
	size_t size = CMTNode_size(p);
	n = nearest_prime(2 * size + 1);
	Vector_init(tab, n);
	for (i = 0; i != p->items.size; ++i) {
		CMTItem* x = p->items.at[i];
		if (!x) {
			continue;
		}
		x->parent = new_parent;
		x->nprobe = 0;
		Data key = Data_maskWith(&x->d, &p->m);
		uint64_t h = hash(&key);
		h1 = hash1(h, n);
		CMTItem* t = tab->at[h1];
		if (t) {
			size_t nprobe = 0;
			bool is_found = false;
			h2 = hash2(h, n);
			do {
				// Search from j to end for a empty position
				++nprobe;
				j = get_probe(h1, h2, nprobe, n);
				if (!tab->at[j]) {
					tab->at[j] = x;
					is_found = true;
					break;
				}
			} while (nprobe != n);
			// Still not found, noisy!
			if (!is_found) {
				printf("CMT_rehash: failed!\n");
			} else if (nprobe > t->nprobe) {
				t->nprobe = nprobe;
			}
		} else {
			tab->at[h1] = x;
		}
	}
}
CMTSCOPE size_t CMTNode_mergeItems(CMTNode* p, size_t beg, size_t end) {
	if (end == beg) {
		return 0; // No item
	}

	CMTItem* x = p->items.at[beg];
	if (!Mask_eq(&x->r->m, &p->m)) {
		if (!x->child) {
			x->child = CMTNode_new(x);
		}
		// Move all rules to child
		CMTItem* new_x = CMTItem_new(&x->r->d, x->child);
		CMTItem_mergeRules(new_x, x);
		CMTNode_addItem(x->child, new_x);
	}

	// More than 1 items
	size_t i;
	if (end != beg + 1) {
		for (i = beg + 1; i != end; ++i) {
			CMTItem* item = p->items.at[i];
			if (Mask_eq(&item->r->m, &p->m)) {
				CMTItem_mergeRules(x, item);
				CMTItem_delete(item);
			} else {
				item->d = item->r->d; //recover from last mask calc
				if (!x->child) {
					x->child = CMTNode_new(x);
				}
				CMTNode_addItem(x->child, item);
			}
		}
	}
	if (x->child) {
		CMTNode_build(x->child);
	}
	return end - beg - 1;
}
static inline CMTItem* CMTNode_xfind(CMTNode* p, Data* d, CMTItem* copy) {
	size_t i, h1, h2, n = p->items.size;
	CMTItem* ret = NULL;
	if (!n) {
		return ret;
	}
	Data key = Data_maskWith(d, &p->m);
	if (n == 1) {
		CMTItem* t = p->items.at[0];
		MEMACC;
		//CMTDBG(".");
		if (t) {
			*copy = *t;
			if (Data_eq(&key, &copy->d)) {
				ret = t;
			}
		}
	} else {
		uint64_t h = hash(&key);
		h1 = hash1(h, n);
		CMTItem* t = p->items.at[h1];
		MEMACC;
		if (t) {
			*copy = *t;
			if (Data_eq(&key, &copy->d)) {
				ret = t;
			} else if (t->nprobe) {
				size_t nprobe = 0;
				h2 = hash2(h, n);
				do {
					++nprobe;
					i = get_probe(h1, h2, nprobe, n);
					CMTItem* x = (CMTItem*) p->items.at[i];
					MEMACC;
					if (!x) {
						break;
					}
					Buf1024 b1, b2;
//					CMTDBG("CMT_xfind: %s, mask:%s, nprobe:%u, %u\n",
//							CMTItem_serialize(x, b1), Mask_serialize(&p->m, b2),
//							nprobe, i);
					*copy = *x;
					if (Data_eq(&key, &copy->d)) {
						ret = x;
						break;
					}
				} while (nprobe != t->nprobe);
			}
		}
	}
	return ret;
}
CMTSCOPE CMTItem* CMTNode_xinsert(CMTNode** pnode, Data* d, CMT* cmt) {
	CMTNode* node = *pnode;
	size_t n = node->items.size;
	CMTItem* x = NULL;
	size_t size = CMTNode_size(node);
	if (4 * size > 3 * n) { // Capacity exceeded
		CMTNode* new_node = CMTNode_new(node->parent);
		new_node->m = node->m;
		CMTNode_rehash(node, &new_node->items, new_node);
		x = CMTNode_xinsert(&new_node, d, cmt);
		CMTItem_replaceChild(node->parent, node, new_node);
		Vector_push(&cmt->free_subnodes_non_recursive, node);
		*pnode = new_node;
	} else {
		Data key = Data_maskWith(d, &node->m);
		x = CMTItem_new(&key, node);
		size_t h1, h2;
		uint64_t h = hash(&key);
		h1 = hash1(h, n);
		CMTItem* t = node->items.at[h1];
		if (t) {
			size_t i;
			size_t nprobe = 0;
			bool is_found = false;
			h2 = hash2(h, n);
			do {
				++nprobe;
				i = get_probe(h1, h2, nprobe, n);
				if (!node->items.at[i]) {
					// TODO: ATOM
					ATOMIC_ASSIGN(node->items.at[i], x);
					is_found = true;
					break;
				}
			} while (nprobe != n);
			if (!is_found) {
				printf("CMTNode_xinsert: failed\n");
			} else {
				if (nprobe > t->nprobe) {
					// TODO: ATOM
					ATOMIC_ASSIGN(t->nprobe, nprobe);
				}
			}
		} else {
			// TODO: ATOM
			ATOMIC_ASSIGN(node->items.at[h1], x);
		}
	}
	return x;
}
CMTSCOPE void CMTNode_collectRules(CMTNode* p, CMTNode* out) {
	size_t i, n = p->items.size;
	for (i = 0; i < n; ++i) {
		CMTItem* item = p->items.at[i];
		if (!item) {
			continue;
		}

		Rule* r = item->r;
		while (r) {
			Rule* next = Rule_next(r);
			CMTNode_addRule(out, r);
			r = next;
		}
		if (item->child) {
			CMTNode_collectRules(item->child, out);
		}
	}

	switch (ptrtype(p->sub_or_ib)) {
	case TY_SUB: {
		CMTNode_collectRules(CMTNode_sub(p), out);
		break;
	}
	case TY_IB: {
		Rule* r = CMTNode_IB(p)->r;
		while (r) {
			Rule* next = Rule_next(r);
			CMTNode_addRule(out, r);
			r = next;
		}
		break;
	}
	}
}
