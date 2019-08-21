/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/in.h>

#define MES_CNT_IDX_START 0
#define MES_CNT_IDX_END 1
#define MES_CNT_SIZE 2

#define MES_DATA_SIZE 512

#define MAP_MES_START BPF_TC_GLOBAL "map_mes_start"
#define MAP_MES_END BPF_TC_GLOBAL "map_mes_end"
#define MAP_MES_CNT BPF_TC_GLOBAL "map_mes_cnt"

#define MES_PROTO IPPROTO_UDP
#define MES_DEST_PORT 0x8913 /* 5001 == 0x1389 -> network order */
