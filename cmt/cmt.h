#pragma once
#include "common.h"
#include "vector.h"

///////////////////////////////////////////////////////////////////////////////
struct _CMTNode { //total 48 bytes
	Mask m;//16 bytes
	Vector items;//12 bytes
	enum {
		TY_SUB = 1, TY_IB = 2,
	};
	uintptr_t sub_or_ib;//4 bytes
	size_t rule_count;//4 bytes
	CMTItem* parent;//4 bytes
	//dave add
    CMTNode* sub_last;//4 bytes
	size_t min_id; // 4 bytes
};

static inline size_t CMTNode_size(CMTNode* p) {
	size_t i, n = 0;
	for (i = 0; i != p->items.size; ++i) {
		if (p->items.at[i]) {
			++n;
		}
	}
	return n;
}
CMTSCOPE CMTNode* CMTNode_new(CMTItem* parent);
CMTSCOPE void CMTNode_delete(CMTNode* p);
CMTSCOPE Rule* CMTNode_find(CMTNode* p, Data* d, OnFind cb, void* par);
CMTSCOPE void CMTNode_build(CMTNode* p);
// Return: Item that wraps [r]
CMTSCOPE CMTItem* CMTNode_addRule(CMTNode* p, Rule* r);
CMTSCOPE void CMTNode_insert(CMTNode* p, Rule* r, CMT* cmt);
CMTSCOPE void CMTNode_rebuild(CMTNode* p, CMT* cmt);
CMTSCOPE void CMTNode_refreshMask(CMTNode* p);

///////////////////////////////////////////////////////////////////////////////
struct _CMTItem {//32 bytes
	Data d; //16 bytes
	size_t nprobe; // 4 bytes
	Rule* r; //corresponding rule link list //4 bytes
	CMTNode* parent;// 4 bytes
	CMTNode* child;// 4 bytes
};
CMTSCOPE CMTItem* CMTItem_new(Data* d, CMTNode* parent);
CMTSCOPE void CMTItem_delete(CMTItem* p);
CMTSCOPE char* CMTItem_serialize(CMTItem* p, char* buf);
CMTSCOPE void CMTItem_addRule(CMTItem* p, Rule* r);
// Merge [item] to [p]
CMTSCOPE void CMTItem_mergeRules(CMTItem* p, CMTItem* item);
CMTSCOPE void CMTItem_removeRule(CMTItem* p, Rule* r, CMT* cmt);
CMTSCOPE void CMTItem_removeChild(CMTItem*p, CMTNode* sub);
CMTSCOPE void CMTItem_replaceChild(CMTItem* p, CMTNode* node, CMTNode* new_node);
///////////////////////////////////////////////////////////////////////////////
struct _CMT {
	CMTItem root;
	Vector free_subnodes;
	Vector free_subnodes_non_recursive;
	Vector free_rule_wrappers;
	bool is_build;
};
CMTSCOPE void CMT_init(CMT* p);
CMTSCOPE void CMT_reset(CMT* p);
CMTSCOPE void CMT_destroy(CMT* p);
CMTSCOPE void CMT_debug(CMT* p, FILE* f, int rule_count);
CMTSCOPE void CMT_flush(CMT* p);
CMTSCOPE bool CMT_isBuild(CMT* p);
CMTSCOPE size_t CMT_getRuleCount(CMT* p);
CMTSCOPE void CMT_build(CMT* p);
static inline Rule* CMT_find(CMT* p, Data* d, OnFind cb, void* par) {
	CMTNode* c = p->root.child; // Must make a copy concerning lock-freedom.
	return c ? CMTNode_find(c, d, cb, par) : NULL;
}
CMTSCOPE void CMT_removeRule(CMT* p, Rule* r);
CMTSCOPE void CMT_insert(CMT* p, Rule* r);
