#include <linux/bpf.h>
#include <linux/pkt_cls.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdint.h>
#include "bpf_elf.h"
#include "mes_chk.h"

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

struct bpf_elf_map mes_map_start __section("maps") = {
	.type		= BPF_MAP_TYPE_ARRAY,
	.size_key	= sizeof(uint32_t),
	.size_value	= sizeof(uint64_t),
	.pinning	= PIN_GLOBAL_NS,
	.max_elem	= MES_DATA_SIZE,
};

struct bpf_elf_map mes_map_end __section("maps") = {
	.type		= BPF_MAP_TYPE_ARRAY,
	.size_key	= sizeof(uint32_t),
	.size_value	= sizeof(uint64_t),
	.pinning	= PIN_GLOBAL_NS,
	.max_elem	= MES_DATA_SIZE,
};

struct bpf_elf_map mes_map_cnt __section("maps") = {
	.type		= BPF_MAP_TYPE_ARRAY,
	.size_key	= sizeof(uint32_t),
	.size_value	= sizeof(uint32_t),
	.pinning	= PIN_GLOBAL_NS,
	.max_elem	= MES_CNT_SIZE,
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
	uint32_t idx, *cnt;
	uint64_t t;

	if (!check_pkt(skb)) {
		return TC_ACT_OK;
	}

	idx = MES_CNT_IDX_START;
	cnt = map_lookup_elem(&mes_map_cnt, &idx);
	if (!cnt) {
		return TC_ACT_OK;
	}
	if (*cnt >= MES_DATA_SIZE) {
		return TC_ACT_OK;
	}

	t = ktime_get_ns();
	idx = *cnt;
	map_update_elem(&mes_map_start, &idx, &t, 0);

	*cnt += 1;

	return TC_ACT_OK;
}

__section("egress")
int mes_end(struct __sk_buff *skb)
{
	uint32_t idx, *cnt;
	uint64_t t;

	if (!check_pkt(skb)) {
		return TC_ACT_OK;
	}

	idx = MES_CNT_IDX_END;
	cnt = map_lookup_elem(&mes_map_cnt, &idx);
	if (!cnt) {
		return TC_ACT_OK;
	}
	if (*cnt >= MES_DATA_SIZE) {
		return TC_ACT_OK;
	}

	t = ktime_get_ns();
	idx = *cnt;
	map_update_elem(&mes_map_end, &idx, &t, 0);

	*cnt += 1;

	return TC_ACT_OK;
}

char __license[] __section("license") = "GPL";
