/* SPDX-License-Identifier: GPL-2.0 */
#include "bpf_defs.h"
#include <linux/ptrace.h>
#include "bpf_eventd.h"

#define PT_REGS_PARM1(x) ((x)->rdi)  /* for x86_64 */

static uint32_t BPF_FUNC(get_smp_processor_id, void);
static uint64_t BPF_FUNC(ktime_get_ns, void);
static int BPF_FUNC(perf_event_output, void *, void *, uint64_t, void *, uint64_t);

/* NOTE: It is assmued that the map file already exists when
 * the program load. Only the map name is meaningful here.
 */
struct bpf_elf_map bpf_sys_cnt __section("maps") = {
	.type		= BPF_MAP_TYPE_PERF_EVENT_ARRAY,
	.size_key	= 4,
	.size_value	= 4,
	.pinning	= PIN_BPF_FS,
	.max_elem	= 1,
};

__section("prog")
int test_prog(struct pt_regs *ctx)
{
	struct bpf_sys_data data;

	data.cmd = (uint32_t)PT_REGS_PARM1(ctx);
	data.cpu = get_smp_processor_id();
	data.time = ktime_get_ns();
	perf_event_output(ctx, &bpf_sys_cnt, BPF_F_CURRENT_CPU,
			   	&data, (uint64_t)sizeof(data));

	return 0;
}
