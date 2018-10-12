#include <linux/in.h>

#define MES_CNT_IDX_START 0
#define MES_CNT_IDX_END 1
#define MES_CNT_SIZE 2

#define MES_DATA_SIZE 512

#define MES_MAP_START "/sys/fs/bpf/tc/globals/mes_map_start"
#define MES_MAP_END "/sys/fs/bpf/tc/globals/mes_map_end"
#define MES_MAP_CNT "/sys/fs/bpf/tc/globals/mes_map_cnt"

#define MES_PROTO IPPROTO_UDP
#define MES_DEST_PORT 0x8913 /* 5001 == 0x1389 -> network order */
