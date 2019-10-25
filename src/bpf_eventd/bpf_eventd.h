/* SPDX-License-Identifier: GPL-2.0 */
#include <stdint.h>

#define MAP_NAME "bpf_sys_cnt"

struct bpf_sys_data {
	uint32_t cmd;
	uint32_t cpu;
	uint64_t time;
};
