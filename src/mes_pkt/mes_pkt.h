#include <linux/in.h>

enum mes_array {
	MES_IDX_COUNT = 0,
	MES_IDX_START,
	MES_IDX_TOTAL,
	MES_IDX_MIN,
	MES_IDX_MAX,
	MES_ARRAY_SIZE
};

#define MES_MAP_PATH "/sys/fs/bpf/tc/globals/mes_data"

#define MES_PROTO IPPROTO_UDP
#define MES_DEST_PORT 0x8913 /* 5001 == 0x1389 -> network order */
