#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <linux/bpf.h>
#include "mes_chk.h"

static int sys_bpf(int cmd, union bpf_attr *attr)
{
	int ret;

	ret = syscall(__NR_bpf, cmd, attr, sizeof(*attr));
	if (ret < 0) { 
		perror("syscall error:");
		exit(1);
	}
	return ret;
}

int main(int argc, char *argv[])
{
	char *path;
	int fd_c, fd_s, fd_e;
	union bpf_attr attr;
	unsigned int cnt_s, cnt_e;
	unsigned long s, e, t, total = 0, min = 0, max = 0, pre_e = 0;
	int idx;
	int vflag = 0, rcnt = 0;

	if (argc == 2 && !strcmp("-v", argv[1])) {
		vflag = 1;
	}

	bzero(&attr, sizeof(attr));
	path = MES_MAP_CNT;
	attr.pathname = (unsigned long)path;
	attr.file_flags = BPF_F_RDONLY;
	fd_c = sys_bpf(BPF_OBJ_GET, &attr);

	bzero(&attr, sizeof(attr));
	path = MES_MAP_START;
	attr.pathname = (unsigned long)path;
	attr.file_flags = BPF_F_RDONLY;
	fd_s = sys_bpf(BPF_OBJ_GET, &attr);

	bzero(&attr, sizeof(attr));
	path = MES_MAP_END;
	attr.pathname = (unsigned long)path;
	attr.file_flags = BPF_F_RDONLY;
	fd_e = sys_bpf(BPF_OBJ_GET, &attr);

	bzero(&attr, sizeof(attr));
	attr.map_fd = fd_c;
	idx = MES_CNT_IDX_START;
	attr.key = (unsigned long)&idx;
	attr.value = (unsigned long)&cnt_s;
	sys_bpf(BPF_MAP_LOOKUP_ELEM, &attr);

	bzero(&attr, sizeof(attr));
	attr.map_fd = fd_c;
	idx = MES_CNT_IDX_END;
	attr.key = (unsigned long)&idx;
	attr.value = (unsigned long)&cnt_e;
	sys_bpf(BPF_MAP_LOOKUP_ELEM, &attr);

	for (idx = 0; idx < cnt_e; idx++) {
		bzero(&attr, sizeof(attr));
		attr.map_fd = fd_s;
		attr.key = (unsigned long)&idx;
		attr.value = (unsigned long)&s;
		sys_bpf(BPF_MAP_LOOKUP_ELEM, &attr);

		bzero(&attr, sizeof(attr));
		attr.map_fd = fd_e;
		attr.key = (unsigned long)&idx;
		attr.value = (unsigned long)&e;
		sys_bpf(BPF_MAP_LOOKUP_ELEM, &attr);

		t = e - s;
		total += t;
		if (min == 0 || t < min) {
			min = t;
		}
		if (t > max) {
			max = t;
		}
		if (vflag) {
			printf("%4d: %lu %lu %lu\n", idx, s, e, t);
		}
		if (s < pre_e) {
			rcnt++;
		}
		pre_e = e;
	}

	printf("reverse count: %d\n", rcnt);
	printf("count: %u/%u, total time: %lu ns, ave. time: %lu ns min: %lu ns max: %lu ns\n",
		cnt_s, cnt_e, total, cnt_e != 0 ? total / cnt_e : 0, min, max);

	return 0;
}
