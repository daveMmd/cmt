#include "cmt.h"
#include "cmt_private.h"

///////////////////////////////////////////////////////////////////////////////
CMTSCOPE CMTNode* CMTNode_new(CMTItem* parent) {
	CMTNode* p = MNEW(sizeof(*p));
	memset(p, 0, sizeof(*p));
	Mask_init(&p->m);
	p->parent = parent;
	return p;
}
CMTSCOPE void CMTNode_delete(CMTNode* p) {
	CMTNode* x = p;
	while (x) {
		CMTItem* ib = CMTNode_IB(x);
		CMTNode* sub = CMTNode_sub(x);
		if (ib) {
			CMTItem_delete(ib);
		}
		Vector_reset(&x->items, (OnErase) CMTItem_delete);
		MDEL(x);
		x = sub;
	}
}

extern size_t current_id;
///////////////////////////////////////////////////////////////////////////////
CMTSCOPE Rule* CMTNode_find(CMTNode* p, Data* d, OnFind cb, void* par) {

	//printf("p->min_id:%ul\n", p->min_id);
	if(current_id < p->min_id) return NULL; //prune nodes and its sub_nodes

	Rule* found = NULL;

	// item may be modified by master thread, must make a copy before visit
	CMTItem item_copy;

//#define RETFOUND if (found) { return found; } //single-match
//#define RETFOUND //test multi-match
	while (p) {
		Rule* found = NULL;
		if (CMTNode_xfind(p, d, &item_copy)) {
			Buf48 b1, b2;
			CMTDBG(
					"CMT_find:{d:%s, m:%s}\n", Data_serialize(&item_copy.d, b1), Mask_serialize(&p->m, b2));
			if (item_copy.r) {
				Rule* it = item_copy.r;
				while (it) {
					found = cb(par, d, it);
					//RETFOUND;
					it = Rule_next(it);
				}
			}
			if (item_copy.child) {
				found = CMTNode_find(item_copy.child, d, cb, par);
				//RETFOUND;
			}
		}

		CMTItem* ib = CMTNode_IB(p);
		if (ib) {
			//CMTDBG("CMT_find: IB, size:%u\n", c->inserted.size);
			item_copy = *ib;
			Rule* it = item_copy.r;
			while (it) {
				Data md = Data_maskWith(d, &it->m);
				if (Data_eq(&md, &it->d)) {
					found = cb(par, d, it);
					//RETFOUND;
				}
				it = Rule_next(it);
			}
		}
		p = CMTNode_sub(p);
		if(p) MEMACC; //subnode or IB pointer, memacc+=1
		if(p && current_id < p->min_id) return found; //prune sub_nodes
	}
#undef RETFOUND
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////

CMTSCOPE int build_cmp(const void* p1, const void* p2) {
	const CMTItem* x1 = *(const CMTItem**) p1;
	const CMTItem* x2 = *(const CMTItem**) p2;
	return Data_cmp(&x1->d, &x2->d);
}
CMTSCOPE void CMTNode_build(CMTNode* p) {
	size_t size = p->items.size;
    //printf("CMTNode_build -- size: %d, p->m:%llu,%llu\n", size, p->m.at[0], p->m.at[1]);
	if (size <= 1) {
		return;
	}

	if (p->parent->parent && Mask_eq(&p->parent->parent->m, &p->m)) {
		CMTNode_cut(p);
		CMTNode_build(p);
		CMTNode* sub_node = CMTNode_sub(p);
		CMTNode_build(sub_node);

		if(p->min_id > sub_node->min_id){
			CMTNode* next_sub_node = CMTNode_sub(sub_node);
			if(next_sub_node != NULL) p->sub_or_ib = makeptr(sub_node->sub_or_ib, TY_SUB);
			else p->sub_or_ib = NULL;
			//p->sub_or_ib = sub_node->sub_or_ib;
			sub_node->sub_or_ib = makeptr(p, TY_SUB);

			CMTNode* sub_last = p->sub_last;
			if(sub_last != NULL) sub_last->sub_or_ib = makeptr(sub_node, TY_SUB);
			else p->parent->child = sub_node;
		}
		return;
	}

	size_t i;

	//dave change calc the rule with highest priority(i.e., smallest id)
	size_t min_id = 0xfffffff;
	for (i = 0; i < size; ++i) {
		CMTItem* item = (CMTItem*) p->items.at[i];
		Rule* last = item->r;
		min_id = last->id < min_id ? last->id : min_id;
		while (ptrtype(last->parent_or_next) == TY_NEXT) {
			last = Rule_next(last);
			min_id = last->id < min_id ? last->id : min_id;
		}
	}
	p->min_id = min_id;

	// Trim()
	for (i = 0; i < size; ++i) {
		CMTItem* x = (CMTItem*) p->items.at[i];
		x->d = Data_maskWith(&x->d, &p->m);
	}

	// Sort()
	Vector_sort(&p->items, build_cmp);

	// Merge()
	size_t pos = 0;
	size_t prev = 0;
	Data memo = ((CMTItem*) p->items.at[0])->d;
	for (i = 1; i != size; ++i) {
		CMTItem* x = (CMTItem*) p->items.at[i];
		if (!Data_eq(&memo, &x->d)) {
			memo = x->d;
			CMTNode_mergeItems(p, prev, i);
			p->items.at[pos++] = p->items.at[prev];
			prev = i;
		}
	}
	CMTNode_mergeItems(p, prev, size);
	p->items.at[pos++] = p->items.at[prev];
	Vector_erase(&p->items, pos, size, NULL);

	// Rehash
	if (p->items.size == 1) {
		Vector_vacuum(&p->items);
	} else {
		Vector out;
		CMTNode_rehash(p, &out, p);
		Vector_swap(&p->items, &out);
		Vector_reset(&out, NULL);
	}
    //printf("CMTNode p->d(%llu,%llu): vec-size(%d), vec-capacity(%d)\n", p->parent->d.at[0], p->parent->d.at[1], p->items.size, p->items.capacity);

}
CMTSCOPE CMTItem* CMTNode_addRule(CMTNode* p, Rule* r) {
	CMTItem* x = CMTItem_new(&r->d, p);
	CMTItem_addRule(x, r);
	CMTNode_addItem(p, x);
	return x;
}
CMTSCOPE void CMTNode_insert(CMTNode* p, Rule* r, CMT* cmt) {
	CMTNode* node = p;
	while (node) {
		if (Mask_contains(&r->m, &node->m)) {
			break;
		}
		node = CMTNode_sub(node);
	}

	if (node) {
		++node->rule_count;
		CMTItem copy;
		CMTItem* x = CMTNode_xfind(node, &r->d, &copy);
		if (!x) {
			x = CMTNode_xinsert(&node, &r->d, cmt);
		}

		if (Mask_eq(&r->m, &node->m)) {
			CMTItem_addRule(x, r);
		} else {
			if (!x->child) {
				CMTNode* c = CMTNode_new(x);
				c->m = r->m;
				CMTNode_addRule(c, r);
				// TODO: ATOM
				ATOMIC_ASSIGN(x->child, c);
			} else {
				CMTNode_insert(x->child, r, cmt);
			}
		}
	} else {
		// Insert rule to lowest sub
		node = p;
		while (CMTNode_sub(node)) {
			node = CMTNode_sub(node);
		}
		++node->rule_count;
		if (!CMTNode_IB(node)) {
			CMTNode_newIB(node);
		}
		CMTItem_addRule(CMTNode_IB(node), r);
	}
}
CMTSCOPE void CMTNode_rebuild(CMTNode* p, CMT* cmt) {
	// DFS
	size_t i, n;

	CMTNode* it = p;
	bool needs_rebuild = false;
	while (it) {
		if (CMTNode_IB(it)) {
			needs_rebuild = true;
			break;
		}
		it = CMTNode_sub(it);
	}

	if (needs_rebuild) {
		CMTNode_verify(p);
		CMTNode* new_node = CMTNode_new(p->parent);
		CMTNode_collectRules(p, new_node);
		CMTNode_build(new_node);

		// TODO: ATOM
		ATOMIC_ASSIGN(p->parent->child, new_node);

		it = p;
		while (it) {
			Vector_push(&cmt->free_subnodes, it);
			it = CMTNode_sub(it);
		}
	} else {
		it = p;
		while (it) {
			n = it->items.size;
			for (i = 0; i < n; ++i) {
				CMTItem* x = it->items.at[i];
				if (x && x->child) {
					// CMTNode_rebuild recurs only on the first child
					CMTNode_rebuild(x->child, cmt);
				}
			}
			it = CMTNode_sub(it);
		}
	}
}
CMTSCOPE void CMTNode_refreshMask(CMTNode* p) {
	size_t i;
	Mask_init(&p->m);
	for (i = 0; i < p->items.size; ++i) {
		CMTItem* x = (CMTItem*) p->items.at[i];
		if (!x) {
			continue;
		}
		p->m = Mask_and(&p->m, &x->r->m);
	}
}
///////////////////////////////////////////////////////////////////////////////
CMTSCOPE CMTItem* CMTItem_new(Data* d, CMTNode* parent) {
	CMTItem* p = (CMTItem*) MNEW(sizeof(*p));
	memset(p, 0, sizeof(*p));
	if (d) {
		p->d = *d;
	}
	p->parent = parent;
	return p;
}

CMTSCOPE void CMTItem_delete(CMTItem* p) {
	if (p->child) {
		CMTNode_delete(p->child);
	}
	MDEL(p);
}
CMTSCOPE char* CMTItem_serialize(CMTItem* p, char* buf) {
	buf[0] = '\0';
	Buf48 b1, b2;
	sprintf(buf,
			"{data:%s, nprobe:%u, addr:%p, parent:%p}", Data_serialize(&p->d, b1), p->nprobe, p, p->parent);
	return buf;
}
CMTSCOPE void CMTItem_addRule(CMTItem* p, Rule* r) {
	if (!p->r) {
		r->parent_or_next = makeptr(p, TY_PARENT);
		// TODO: ATOM
		ATOMIC_ASSIGN(p->r, r);
	} else {
		r->parent_or_next = makeptr(p->r, TY_NEXT);
		// TODO: ATOM
		ATOMIC_ASSIGN(p->r, r);
	}
}
CMTSCOPE void CMTItem_mergeRules(CMTItem* p, CMTItem* item) {
	Rule* last = item->r;
	while (ptrtype(last->parent_or_next) == TY_NEXT) {
		last = Rule_next(last);
	}
	if (!p->r) {
		last->parent_or_next = makeptr(p, TY_PARENT);
	} else {
		last->parent_or_next = makeptr(p->r, TY_NEXT);
	}
	p->r = item->r;
	item->r = NULL;
}

CMTSCOPE void CMTItem_removeRule(CMTItem* p, Rule* r, CMT* cmt) {
	bool is_removed = false;

	Rule sentinel;
	sentinel.parent_or_next = makeptr(p->r, TY_NEXT);
	Rule* pre = &sentinel;

	do {
		Rule* cur = Rule_next(pre);
		if (cur == r) {
			// TODO: ATOM
			ATOMIC_ASSIGN(pre->parent_or_next, cur->parent_or_next);
			is_removed = true;
			break;
		}
		pre = cur;
	} while (pre);

	if (!is_removed) {
		printf("CMTItem_removeRule failed, rid:%u!\n", r->id);
		printf("%p, %p\n", p, Rule_parent(r));
		return; // Should never get here
	}
	// TODO: ATOM
	ATOMIC_ASSIGN(p->r, Rule_next(&sentinel));

	CMTNode* node = p->parent;
	CMTNode* top_empty_node = NULL;

	while (node) {
		--node->rule_count;
		if (!node->rule_count) {
			top_empty_node = node;
		}
		node = node->parent->parent;
	}

	if (top_empty_node) {
		CMTItem_removeChild(top_empty_node->parent, top_empty_node);
		Vector_push(&cmt->free_subnodes, top_empty_node);
	}
}
CMTSCOPE void CMTItem_removeChild(CMTItem*p, CMTNode* child) {
	CMTNode sentinel;
	sentinel.sub_or_ib = makeptr(p->child, TY_SUB);
	CMTNode* pre = &sentinel;
	CMTNode* cur = CMTNode_sub(pre);
	bool is_removed = false;
	while (cur) {
		if (cur == child) {
			// IB should not be assigned
			// TODO: ATOM
			ATOMIC_ASSIGN(pre->sub_or_ib, makeptr(CMTNode_sub(cur), TY_SUB));
			is_removed = true;
			break;
		}
		pre = cur;
		cur = CMTNode_sub(cur);
//		printf("%p\n", cur);
	}
	if (!is_removed) {
		printf("CMTItem_removeChild failed!\n");
		return;
	}
	// TODO: ATOM
	ATOMIC_ASSIGN(p->child, CMTNode_sub(&sentinel));
}
CMTSCOPE void CMTItem_replaceChild(CMTItem* p, CMTNode* node, CMTNode* new_node) {
	new_node->parent = p;
	new_node->rule_count = node->rule_count;
	new_node->sub_or_ib = node->sub_or_ib;
	CMTItem* ib = CMTNode_IB(new_node);
	if (ib) {
		ib->parent = new_node;
	}
	CMTNode sentinel;
	sentinel.sub_or_ib = makeptr(p->child, TY_SUB);
	CMTNode* pre = &sentinel;
	CMTNode* cur = CMTNode_sub(pre);
	bool is_replaced = false;
	while (cur) {
		if (cur == node) {
			// TODO: ATOM
			ATOMIC_ASSIGN(pre->sub_or_ib, makeptr(new_node, TY_SUB));
			is_replaced = true;
			break;
		}
		pre = cur;
		cur = CMTNode_sub(cur);
	}
	if (!is_replaced) {
		printf("CMTItem_replaceChild failed!\n");
		return;
	}
	// TODO: ATOM
	ATOMIC_ASSIGN(p->child, CMTNode_sub(&sentinel));
}
///////////////////////////////////////////////////////////////////////////////
CMTSCOPE void CMT_init(CMT* p) {
	memset(p, 0, sizeof(*p));
}
CMTSCOPE void CMT_reset(CMT* p) {
	CMTNode* c = p->root.child;
	if (c) {
		// TODO: ATOM
		ATOMIC_ASSIGN(p->root.child, NULL);
		do {
			Vector_push(&p->free_subnodes, c);
			c = CMTNode_sub(c);
		} while (c);
	}
	p->is_build = false;
}
CMTSCOPE void CMT_destroy(CMT* p) {
	CMT_flush(p);
	if (p->root.child) {
		CMTNode_delete(p->root.child);
		p->root.child = NULL;
	}
}
CMTSCOPE void CMT_debug(CMT* p, FILE* f, int rule_count) {
	CMTDebugInfo info;
	memset(&info, 0, sizeof(info));
	if (p->root.child) {
		CMTNode_debug(p->root.child, f, &info, 1); //root node depth=1
	}
	fprintf(f, "node_count:%u, rule_count:%u, subnode_count:%u\n",
			info.node_count, info.rule_count, info.subnode_count);
	fprintf(f, "max_depth(height)=%d, average_depth=%f, average_sub_depth=%f\n", info.depth, info.overall_depth*1.0/info.rule_count, info.sub_depth*1.0/info.subnode_count);
	int itemsize = 25, nodesize = 41;
	int memsize = info.item_count * itemsize + info.node_count * nodesize;
	printf("memsize in therotics:%d\n", memsize);
	printf("average mem per rule(bytes):%f\n", memsize*1.0 / rule_count);
//	fprintf(stdout, "%u\t\t%u\t\t%u\t\t%.2f%%\n", info.rule_count, info.node_count,
//			info.subnode_count,
//			(double) info.subnode_count / info.node_count * 100);
}

CMTSCOPE void subnode_delete(CMTNode* p) {
	CMTItem* ib = CMTNode_IB(p);
	if (ib) {
		CMTItem_delete(ib);
	}
	Vector_reset(&p->items, (OnErase) CMTItem_delete);
	MDEL(p);
}
CMTSCOPE void subnode_delete_non_recursive(CMTNode* p) {
	Vector_reset(&p->items, NULL);
	MDEL(p);
}
CMTSCOPE void rule_wrapper_delete(CMTRuleWrapper* p) {
	MDEL(p);
}
CMTSCOPE void CMT_flush(CMT* p) {
	bool need_flush = p->free_subnodes.size
			|| p->free_subnodes_non_recursive.size
			|| p->free_rule_wrappers.size;
	if (need_flush) {
		USLEEP(100000);
		Vector_reset(&p->free_subnodes, (OnErase) subnode_delete);
		Vector_reset(&p->free_subnodes_non_recursive,
				(OnErase) subnode_delete_non_recursive);
		Vector_reset(&p->free_rule_wrappers, (OnErase) rule_wrapper_delete);
	}
}
CMTSCOPE bool CMT_isBuild(CMT* p) {
	return p->is_build;
}
CMTSCOPE size_t CMT_getRuleCount(CMT* p) {
	CMTNode* c = p->root.child;
	return c ? c->rule_count : 0;
}
CMTSCOPE void CMT_build(CMT* p) {
	CMTNode* c = p->root.child;
	if (c) {
		if (!p->is_build) {
			CMTNode_refreshMask(c);
			CMTNode_build(c);
		} else {
			CMTNode_rebuild(c, p);
		}
	}
	p->is_build = true;
}
CMTSCOPE void CMT_removeRule(CMT* p, Rule* r) {
	CMTItem* item = Rule_parent(r);
	if (item) {
		CMTItem_removeRule(item, r, p);
		r->parent_or_next = 0;
	}
}
CMTSCOPE void CMT_insert(CMT* p, Rule* r) {
	if (!p->root.child) {
		// TODO: ATOM
		ATOMIC_ASSIGN(p->root.child, CMTNode_new(&p->root));
	}
	CMTNode* c = p->root.child;
	if (!p->is_build) {
		// Not built yet
		CMTNode_addRule(c, r);
	} else {
		CMTNode_insert(c, r, p);
	}
}
