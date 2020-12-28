#pragma once

#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include "cmt/cmt_inline.h"

#define CLASSBENCH_FLTR argv[1]//"/Users/teng/Developer/cmb/MyFilters10k"
#define CLASSBENCH_TRCE argv[2]//"/Users/teng/Developer/cmb/MyFilters10k_trace"
#define CLASSBENCH_HEAPSIZE ((2<<30)-1)

///////////////////////////////////////////////////////////////////////////////
typedef struct _IPHeader {
	uint32_t sip;
	uint32_t dip;
	uint16_t sport;
	uint16_t dport;
	uint8_t proto;
	uint8_t _pad[3];
} IPHeader;
static inline char* IPHeader_serialize(IPHeader* ff, char* buf) {
	buf[0] = '\0';
	sprintf(buf,
			"{sip:%08x, dip:%08x, sp:%u, dp:%u, proto:%u}", ff->sip, ff->dip, ff->sport, ff->dport, ff->proto);
	return buf;
}

///////////////////////////////////////////////////////////////////////////////
typedef struct _IPRule {
	uint32_t rule_id;
	uint32_t sip;
	uint32_t sip_mask;
	uint32_t dip;
	uint32_t dip_mask;
	uint16_t sport_min;
	uint16_t sport_max;
	uint16_t dport_min;
	uint16_t dport_max;
	uint8_t proto;
	uint8_t proto_mask;
	uint8_t active; /* reserve */
	uint16_t action; /* None_match:0, FWD:1, Drop:2 */
} IPRule;

///////////////////////////////////////////////////////////////////////////////
typedef struct _ClassBench {
	size_t rule_count; //S
	size_t trace_count; //T
	double preprocessing_time; //P
	double lookup_time; //L
	size_t memory_cost; //M
	int memory_access; //A
} ClassBench;
static inline double ClassBench_tick() {
	static struct timeval tick = { 0 };
	struct timeval tock;
	gettimeofday(&tock, NULL);
	double diff = (tock.tv_sec - tick.tv_sec)
			+ (double) (tock.tv_usec - tick.tv_usec) / 1000000;
	tick = tock;
	return diff;
}
static inline Vector ClassBench_getRule(char* file_name) {
	Vector ru;
	Vector_init(&ru, 0);
	size_t id = 0;
	Buf1024 ln;
	FILE* fp = fopen(file_name, "r");
	if (!fp) {
		return ru;
	}
	while (fgets(ln, sizeof(ln), fp)) {
#define IPFMT "%u.%u.%u.%u/%u"
#define PORTFMT "%u : %u"
#define PROTFMT "%x/%x"
#define IP(a, b, c, d) ((a << 24) + (b << 16) + (c << 8) + d)
#define FMT "@"IPFMT"\t"IPFMT"\t"PORTFMT"\t"PORTFMT"\t"PROTFMT
		unsigned a, b, c, d, e;
		unsigned f, g, h, i, j;
		unsigned k, l;
		unsigned m, n;
		unsigned o, p;
		sscanf(ln, FMT, &a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n,
				&o, &p);

		IPRule* rule = malloc(sizeof(*rule));

		rule->rule_id = id;
		rule->sip = IP(a, b, c, d);
		rule->sip_mask = bit_to_mask32(e);

		rule->dip = IP(f, g, h, i);
		rule->dip_mask = bit_to_mask32(j);

		rule->sport_min = k;
		rule->sport_max = l;

		rule->dport_min = m;
		rule->dport_max = n;

		rule->proto = o;
		rule->proto_mask = p;

		Vector_push(&ru, rule);
		++id;
#undef IPFMT
#undef PORTFMT
#undef PROTFMT
#undef IP
#undef FMT
	}
	fclose(fp);
	return ru;
}

static inline Vector ClassBench_getTrace(char* file_name) {
    Buf1024 ln;
    Vector tr;
    Vector_init(&tr, 0);
    size_t count = 0;
    FILE* fp = fopen(file_name, "r");
    if (!fp) {
        return tr;
    }
    while (fgets(ln, sizeof(ln), fp)) {
        unsigned a, b;
        unsigned c, d;
        unsigned e, f, g;
#define IPFMT "%u"
#define PORTFMT "%u"
#define PROTFMT "%u"
//#define FMT IPFMT"\t"IPFMT"\t"PORTFMT"\t"PORTFMT"\t"PROTFMT
#define FMT IPFMT"\t"IPFMT"\t"PORTFMT"\t"PORTFMT"\t"PROTFMT"\t"PORTFMT"\t"PROTFMT
        sscanf(ln, FMT, &a, &b, &c, &d, &e, &f, &g);
        IPHeader* ff = malloc(sizeof(*ff));
        memset(ff, 0, sizeof(*ff));

        ff->sip = a;
        ff->dip = b;
        ff->sport = c;
        ff->dport = d;
        ff->proto = e;

        Vector_push(&tr, ff);
        ++count;
#undef IPFMT
#undef PORTFMT
#undef PROTFMT
#undef FMT
    }
    fclose(fp);
    return tr;
}

static inline Vector ClassBench_getTrace2(char* file_name, Vector* valid_res) {
	Buf1024 ln;
	Vector tr;
	Vector_init(&tr, 0);
	size_t count = 0;
	FILE* fp = fopen(file_name, "r");
	if (!fp) {
		return tr;
	}
	while (fgets(ln, sizeof(ln), fp)) {
		unsigned a, b;
		unsigned c, d;
		unsigned e, f, g;
#define IPFMT "%u"
#define PORTFMT "%u"
#define PROTFMT "%u"
//#define FMT IPFMT"\t"IPFMT"\t"PORTFMT"\t"PORTFMT"\t"PROTFMT
#define FMT IPFMT"\t"IPFMT"\t"PORTFMT"\t"PORTFMT"\t"PROTFMT"\t"PORTFMT"\t"PROTFMT
		sscanf(ln, FMT, &a, &b, &c, &d, &e, &f, &g);
		IPHeader* ff = malloc(sizeof(*ff));
		memset(ff, 0, sizeof(*ff));

		ff->sip = a;
		ff->dip = b;
		ff->sport = c;
		ff->dport = d;
		ff->proto = e;

		Vector_push(&tr, ff);

		int* res = malloc(sizeof(int));
		*res = g;
		Vector_push(valid_res, res);
		++count;
#undef IPFMT
#undef PORTFMT
#undef PROTFMT
#undef FMT
	}
	fclose(fp);
	return tr;
}
static inline void ClassBench_free(Vector* v) {
	Vector_reset(v, free);
}
static inline void ClassBench_print(ClassBench* p, char* name) {
	/*printf(".%s={.S=%u, .T=%u, .P=%lf, "
			".L=%lf, .M=%u, .A=%d},\n average memory accesses:%f\n", name,
			p->rule_count, p->trace_count, p->preprocessing_time,
			p->lookup_time, p->memory_cost, p->memory_access, p->memory_access*1.0/p->trace_count);*/
	printf("rule-set size:%u\n", p->rule_count);
	printf("trace count:%u\n", p->trace_count);
	printf("preprocessing-time:%lf\n", p->preprocessing_time);
	printf("total lookup time:%lf\n", p->lookup_time);
	printf("average memory accesses:%f\n", p->memory_access*1.0/p->trace_count);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
