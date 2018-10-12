#include <linux/bpf.h>
#include <linux/pkt_cls.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdint.h>
#include "bpf_elf.h"
#include "mes_pkt.h"

#ifndef __section
#define __section(NAME) \
__attribute__((section(NAME), used))
#endif

#ifndef __inline
#define __inline \
inline __attribute((always_inline))
#endif

#ifndef BPF_FUNC
#define BPF_FUNC(NAME, ...) \
(*NAME)(__VA_ARGS__) = (void *)BPF_FUNC_##NAME
#endif

static void *BPF_FUNC(map_lookup_elem, void *, void *);
static int BPF_FUNC(map_update_elem, void *, void *, void *, int);
static uint64_t BPF_FUNC(ktime_get_ns, void);

struct bpf_elf_map mes_data __section("maps") = {
	.type		= BPF_MAP_TYPE_ARRAY,
	.size_key	= sizeof(uint32_t),
	.size_value	= sizeof(uint64_t),
	.pinning	= PIN_GLOBAL_NS,
	.max_elem	= MES_ARRAY_SIZE,
};


static __inline int check_pkt(struct __sk_buff *skb)
{
	void *data = (void *)(long)skb->data;
	void *data_end = (void *)(long)skb->data_end;
	struct ethhdr *eth = data;
	struct iphdr *ip = data + sizeof(*eth);
	struct udphdr *udp = (void *)ip + sizeof(*ip);

	if (data + sizeof(*eth) + sizeof(*ip) + sizeof(*udp) > data_end) {
		return 0;
	}
	if (eth->h_proto == 0x08 && ip->protocol == MES_PROTO &&
	    udp->dest == MES_DEST_PORT) {
		return 1;
	}
	return 0;
}

__section("ingress")
int mes_start(struct __sk_buff *skb)
{
	uint32_t idx = MES_IDX_START;
	uint64_t t;

	if (!check_pkt(skb)) {
		return TC_ACT_OK;
	}

	t = ktime_get_ns();
	map_update_elem(&mes_data, &idx, &t, 0);

	return TC_ACT_OK;
}

__section("egress")
int mes_end(struct __sk_buff *skb)
{
	uint32_t idx;
	uint64_t *cnt, *start, end, t, *total, *min, *max;

	if (!check_pkt(skb)) {
		return TC_ACT_OK;
	}

	end = ktime_get_ns();

	idx = MES_IDX_COUNT;
	cnt = map_lookup_elem(&mes_data, &idx);
	if (cnt) {
		*cnt += 1;
	}

	idx = MES_IDX_START;
	start = map_lookup_elem(&mes_data, &idx);
	if (start) {
		t = end - *start;
	} else {
		t = 0;
	}

	idx = MES_IDX_TOTAL;
	total = map_lookup_elem(&mes_data, &idx);
	if (total) {
		*total += t;
	}

	idx = MES_IDX_MIN;
	min = map_lookup_elem(&mes_data, &idx);
	if (min && (*min == 0 || t < *min)) {
		*min = t;
	}

	idx = MES_IDX_MAX;
	max = map_lookup_elem(&mes_data, &idx);
	if (max && t > *max) {
		*max = t;
	}

	return TC_ACT_OK;
}

char __license[] __section("license") = "GPL";
