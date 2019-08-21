/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __BPF_DEFS__
#define __BPF_DEFS__

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/bpf.h>
#include <stdint.h>

#define BPF_FS "/sys/fs/bpf/"
#define BPF_TC_GLOBAL "/sys/fs/bpf/tc/globals/"

static inline int bpf_sys(int cmd, union bpf_attr *attr)
{
	return syscall(__NR_bpf, cmd, attr, sizeof(*attr));
}

/* ELF map definition
 *
 * This definition follows convention used in tc (etc.).
 *
 */
#define PIN_NONE	0
#define PIN_BPF_FS	1  /* pin under /sys/fs/bpf/ */
#define PIN_TC_GLOBAL	2  /* pin under /sys/fs/bpf/tc/globals/ */

struct bpf_elf_map {
	uint32_t type;
	uint32_t size_key;
	uint32_t size_value;
	uint32_t max_elem;
	uint32_t flags;
	uint32_t id;
	uint32_t pinning;
	uint32_t inner_id;
	uint32_t inner_idx;
};

/* definition for eBPF program written in C */

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

#endif /* __BPF_DEFS__ */
