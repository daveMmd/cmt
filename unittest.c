#define CMT_BIT 64
#define CMT_DIM 2
#define NAME "CMT"

#include "classbench.h"
#define FLTR CLASSBENCH_FLTR
#define TRCE CLASSBENCH_TRCE
//#define HEAPSIZE CLASSBENCH_HEAPSIZE
#define HEAPSIZE ((2<<30)-1)

CMT cmt;
Vector ru;
Vector tr;
ClassBench bench = { 0 };
size_t current_id;
size_t false_postive=0;
///////////////////////////////////////////////////////////////////////////////

typedef struct _TestRule {
	Rule r;
	uint16_t dport[2];
	uint16_t sport[2];
} TestRule;
TestRule* TestRule_new() {
	TestRule* p = (TestRule*) MNEW(sizeof(*p));
	memset(p, 0, sizeof(*p));
	return p;
}
void TestRule_delete(void* p) {
	MDEL(p);
}
char* TestRule_serialize(TestRule* p, char* buf) {
	buf[0] = '\0';
	Buf48 b1;
	sprintf(buf,
			"{r:%s, sp:%X~%X, dp:%X~%X}", Rule_serialize(&p->r, b1), p->sport[0], p->sport[1], p->dport[0], p->dport[1]);
	return buf;
}
void TestRule_fromIPRule(TestRule* p, IPRule* r) {
	p->r.id = r->rule_id;

	IPHeader* d = (IPHeader*) &p->r.d;
	IPHeader* m = (IPHeader*) &p->r.m;

	d->sip = r->sip;
	m->sip = r->sip_mask;

	d->dip = r->dip;
	m->dip = r->dip_mask;

	interval_to_pc16(r->sport_min, r->sport_max, &d->sport, &m->sport);
	p->sport[0] = r->sport_min;
	p->sport[1] = r->sport_max;

	interval_to_pc16(r->dport_min, r->dport_max, &d->dport, &m->dport);
	p->dport[0] = r->dport_min;
	p->dport[1] = r->dport_max;

	d->proto = r->proto;
	m->proto = r->proto_mask;
	Rule_trim(&p->r);
}

Rule* onFind(void* par, Data* d, Rule* r) {
    MEMACC; //each rule add one access
	Buf48 b1, b2;
	IPHeader* f = (IPHeader*) d;
	TestRule* m = CONTAINER(r, TestRule, r);
	bool is_match = false;
	if (f->dport >= m->dport[0] && f->dport <= m->dport[1]
			&& f->sport >= m->sport[0] && f->sport <= m->sport[1]) {
		is_match = true;
	}

	CMTDBG(
			"onFind%d: {d:%s, r:%s, r_addr:%p\n", is_match, Data_serialize(d, b1), TestRule_serialize(m, b2), r);
	if (is_match) {
        if(current_id > r->id) current_id = r->id;
		return r;
	}
    else{
        false_postive++;
    }
	return NULL;
}

//static inline void onGetTrace(IPHeader* ff) {
static inline size_t onGetTrace(IPHeader* ff) {
	CMTDBG("+++++++++++++++++++++++++++++++\n");
    current_id = 0xfffffff;// init inf
	Buf1024 b1;
	CMTDBG("%s\n", Data_serialize((Data*) ff, b1));
	Rule* r = CMT_find(&cmt, (Data*) ff, onFind, NULL);
	//printf("current_id:%d ", current_id);
	return current_id;
	if (r) {
		TestRule* t = CONTAINER(r, TestRule, r);
		CMTDBG("FOUND: %s\n", TestRule_serialize(t, b1));
	}
	CMTDBG("-------------------------------\n");
}

void* classbench_thread(void* p) {
	size_t i, x = 1;
	while (x--) {
		for (i = 0; i != tr.size; ++i) {
			onGetTrace((IPHeader*) tr.at[i]);
		}
	}
	return NULL;
}

void test_update(char* trace) {
	tr = ClassBench_getTrace(trace);
	CMT_build(&cmt);
	size_t i;
	const size_t t_size = 1;
	pthread_t t[t_size];
	for (i = 0; i < t_size; ++i) {
		int result = pthread_create(&t[i], NULL, classbench_thread, &cmt);
		result = 0;
	}
	USLEEP(1);

	size_t half = ru.size / 2;
	// Performing insertions
	for (i = half; i < ru.size; ++i) {
		TestRule* x = ru.at[i];
		CMT_insert(&cmt, &x->r);
		if (!(i % 1000)) {
			printf(".");
			CMT_build(&cmt);
			CMT_flush(&cmt);
			printf(".\n");
		}
	}

	for (i = 0; i < ru.size; ++i) {
		TestRule* x = ru.at[i];
		CMT_removeRule(&cmt, &x->r);
		if (!(i % 1000)) {
			CMT_flush(&cmt);
			printf("%zu\n", i);
		}
	}

	for (i = 0; i < t_size; ++i) {
		pthread_join(t[i], NULL);
	}

	printf("Main thread finished!\n");
	CMT_flush(&cmt);
	ClassBench_free(&tr);
}

void test_find() {
	//////
	Data d;
	Buf1024 b1;
	IPHeader* ff = (IPHeader*) &d;
	ff->sip = 0x42b24ce2; //66.178.76.226
	ff->dip = 0x186cff2a; //24.108.255.42
	ff->sport = 1324;
	ff->dport = 1705;
	ff->proto = 6;
	CMTDBG("%s {\n", Data_serialize(&d, b1));
	CMT_find(&cmt, &d, onFind, NULL);
	CMTDBG("}\n");
}

__attribute__ ((visibility("default")))
int main(int argc, char* argv[]) {

	srand(time(0));

	size_t i;
	heap_init(malloc(HEAPSIZE), HEAPSIZE);
	CMT_init(&cmt);

	// Load rules and traces
	Vector ip_rules = ClassBench_getRule(FLTR);
	Vector_init(&ru, ip_rules.size);
	for (i = 0; i != ru.size; ++i) {
		TestRule* x = TestRule_new();
		TestRule_fromIPRule(x, (IPRule*) ip_rules.at[i]);
		ru.at[i] = x;
	}
	ClassBench_free(&ip_rules);
	bench.rule_count = ru.size;

	///////////////////////////////////////////////////////////////////////////
//	test_update();
//	return 0;

	// Preprocessing time
	ClassBench_tick();
	for (i = 0; i != ru.size; ++i) {
		TestRule* x = ru.at[i];
		CMT_insert(&cmt, &x->r);
	}
//	return 0;
	CMT_build(&cmt);
	bench.preprocessing_time = ClassBench_tick();
	CMT_debug(&cmt, fopen("dbg.txt", "w"), bench.rule_count);
//	exit(0);

//	FILE* f = fopen("x", "w");
//	CMT_debug(&cmt, f);
//	return 0;

//	test_find();
//	return 0;

	// Memory usage
	heap_debug(&bench.memory_cost);

	// Lookup time
	Vector valid_res;
	Vector_init(&valid_res, 0);
	int mis_match = 0;

	tr = ClassBench_getTrace2(TRCE, &valid_res);
	bench.trace_count = tr.size;
	ClassBench_tick();
	for (i = 0; i != tr.size; ++i) {
		size_t res_id = onGetTrace((IPHeader*) tr.at[i]);
		/*int valid_res_id = *(int*)valid_res.at[i];
		if(res_id != valid_res_id){
		    mis_match++;
		}*/
	}
	bench.lookup_time = ClassBench_tick();
	bench.memory_access = mem_acc;
	ClassBench_free(&tr);

	// Output
	//printf("mis matches:%d\n", mis_match);
	ClassBench_print(&bench, NAME);
    printf("false postives:%u average false positives:%f\n", false_postive, false_postive*1.0/bench.trace_count);

	///////////////////////////////////////////////////////////////////////////
	CMT_destroy(&cmt);
	Vector_reset(&ru, TestRule_delete);

//	heap_debug(NULL);

	return 0;
}

