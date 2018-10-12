#include <linux/bpf.h>
#include <linux/pkt_cls.h>
#include <linux/ptrace.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdint.h>
#include "bpf_elf.h"

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

/* for x86_64 only */
#define PT_REGS_PARM1(x) ((x)->rdi)
#define PT_REGS_PARM2(x) ((x)->rsi)
#define PT_REGS_PARM3(x) ((x)->rdx)

static void *BPF_FUNC(map_lookup_elem, void *, void *);
static int BPF_FUNC(probe_read, void *, int, void *);

struct bpf_elf_map test_map_cnt __section("maps") = {
	.type		= BPF_MAP_TYPE_ARRAY,
	.size_key	= sizeof(uint32_t),
	.size_value	= sizeof(uint32_t),
	.pinning	= PIN_GLOBAL_NS,
	.max_elem	= 1,
};

/* target: veth_xmit */

__section("prog")
int test_prog(struct pt_regs *ctx)
{
	uint32_t idx, *cnt;
	uint64_t sk_p = (uint64_t)PT_REGS_PARM1(ctx);
	uint64_t data_p;
	uint16_t port;
	int ret;

	/* sk_p: struct sk_buff * */
	/* 216 == offsetof(struct sk_buff, data) */
	ret = probe_read((void *)&data_p, 8, (void *)(sk_p + 216));
	if (ret == 0) {
		/* data_p: head pointer of packet */
		/* 36 == sizeof(struct ethhdr) + sizeof(struct iphdr) +
			 offsetof(struct udphdr, dest) */
		ret = probe_read((void *)&port, 2, (void *)(data_p + 36));
		/* 0x8913: 5001 == 0x1389 -> network byte order */
		if (ret == 0 && port == 0x8913) {
			idx = 0;
			cnt = map_lookup_elem(&test_map_cnt, &idx);
			if (cnt) {
				*cnt += 1;
			}
		}
	}

	return 0;
}

char __license[] __section("license") = "GPL";
