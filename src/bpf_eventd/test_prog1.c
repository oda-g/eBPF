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

static uint32_t BPF_FUNC(get_smp_processor_id, void);
static uint64_t BPF_FUNC(ktime_get_ns, void);
static int BPF_FUNC(perf_event_output, void *, void *, uint64_t, void *, uint64_t);

struct bpf_elf_map map_event __section("maps") = {
	.type		= BPF_MAP_TYPE_PERF_EVENT_ARRAY,
	.size_key	= sizeof(uint32_t),
	.size_value	= sizeof(uint32_t),
	.pinning	= PIN_OBJECT_NS,
	.max_elem	= 4,
};

__section("prog")
int test_prog(struct pt_regs *ctx)
{
	struct {
		short idx;
		short cmd;
		uint32_t cpu;
		uint64_t time;
	} data;

	data.idx = 1;
	data.cmd = (short)PT_REGS_PARM1(ctx);
	data.cpu = get_smp_processor_id();
	data.time = ktime_get_ns();
	perf_event_output(ctx, &map_event, BPF_F_CURRENT_CPU,
			   	&data, (uint64_t)sizeof(data));

	return 0;
}

char __license[] __section("license") = "GPL";
