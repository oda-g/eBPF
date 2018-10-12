#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <linux/bpf.h>
#include "mes_pkt.h"

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
	char *path = MES_MAP_PATH;
	int fd;
	union bpf_attr attr;
	unsigned long cnt, total, min, max;
	int idx;

	bzero(&attr, sizeof(attr));
	attr.pathname = (unsigned long)path;
	attr.file_flags = BPF_F_RDONLY;
	fd = sys_bpf(BPF_OBJ_GET, &attr);

	bzero(&attr, sizeof(attr));
	attr.map_fd = fd;
	idx = MES_IDX_COUNT;
	attr.key = (unsigned long)&idx;
	attr.value = (unsigned long)&cnt;
	sys_bpf(BPF_MAP_LOOKUP_ELEM, &attr);

	bzero(&attr, sizeof(attr));
	attr.map_fd = fd;
	idx = MES_IDX_TOTAL;
	attr.key = (unsigned long)&idx;
	attr.value = (unsigned long)&total;
	sys_bpf(BPF_MAP_LOOKUP_ELEM, &attr);

	bzero(&attr, sizeof(attr));
	attr.map_fd = fd;
	idx = MES_IDX_MIN;
	attr.key = (unsigned long)&idx;
	attr.value = (unsigned long)&min;
	sys_bpf(BPF_MAP_LOOKUP_ELEM, &attr);

	bzero(&attr, sizeof(attr));
	attr.map_fd = fd;
	idx = MES_IDX_MAX;
	attr.key = (unsigned long)&idx;
	attr.value = (unsigned long)&max;
	sys_bpf(BPF_MAP_LOOKUP_ELEM, &attr);

	printf("count: %ld, total time: %lu ns, ave. time: %lu ns min: %lu ns max: %lu ns\n",
		cnt, total, cnt != 0 ? total / cnt : 0, min, max);

	return 0;
}
