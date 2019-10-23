/* SPDX-License-Identifier: GPL-2.0 */
#include "bpf_defs.h"
#include <linux/ptrace.h>

#define PT_REGS_PARM1(x) ((x)->rdi)  /* for x86_64 */

static void *BPF_FUNC(map_lookup_elem, void *, void *);

struct bpf_elf_map test_map_cnt __section("maps") = {
	.type		= BPF_MAP_TYPE_ARRAY,
	.size_key	= sizeof(uint32_t),
	.size_value	= sizeof(uint32_t),
	.pinning	= PIN_BPF_FS,
	.max_elem	= 1,
};

__section("prog")
int test_prog(struct pt_regs *ctx)
{
	uint32_t idx, *cnt;
	int cmd = (int)PT_REGS_PARM1(ctx);

	idx = 0;
	cnt = map_lookup_elem(&test_map_cnt, &idx);
	if (cnt) {
		if (cmd == BPF_PROG_GET_FD_BY_ID) {
			*cnt += 1;
		}
	}

	return 0;
}
