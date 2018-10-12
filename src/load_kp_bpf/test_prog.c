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
static int BPF_FUNC(map_update_elem, void *, void *, void *, int);
static uint64_t BPF_FUNC(ktime_get_ns, void);

struct bpf_elf_map test_map_time __section("maps") = {
	.type		= BPF_MAP_TYPE_ARRAY,
	.size_key	= sizeof(uint32_t),
	.size_value	= sizeof(uint64_t),
	.pinning	= PIN_GLOBAL_NS,
	.max_elem	= 1,
};

struct bpf_elf_map test_map_cnt __section("maps") = {
	.type		= BPF_MAP_TYPE_ARRAY,
	.size_key	= sizeof(uint32_t),
	.size_value	= sizeof(uint32_t),
	.pinning	= PIN_GLOBAL_NS,
	.max_elem	= 1,
};

__section("prog")
int test_prog(struct pt_regs *ctx)
{
	uint32_t idx, *cnt;
	uint64_t t;
	int cmd = (int)PT_REGS_PARM1(ctx);

	idx = 0;
	cnt = map_lookup_elem(&test_map_cnt, &idx);
	if (cnt) {
		if (cmd == BPF_PROG_LOAD) {
			*cnt += 1;
		}
	}

	t = ktime_get_ns();
	idx = 0;
	map_update_elem(&test_map_time, &idx, &t, 0);

	return 0;
}

char __license[] __section("license") = "GPL";
