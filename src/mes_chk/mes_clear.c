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
	int fd;
	union bpf_attr attr;
	unsigned int cnt = 0;
	unsigned long data = 0;
	int idx;

	bzero(&attr, sizeof(attr));
	path = MES_MAP_CNT;
	attr.pathname = (unsigned long)path;
	attr.file_flags = BPF_F_WRONLY;
	fd = sys_bpf(BPF_OBJ_GET, &attr);

	for (idx = 0; idx < MES_CNT_SIZE; idx++) {
		bzero(&attr, sizeof(attr));
		attr.map_fd = fd;
		attr.key = (unsigned long)&idx;
		attr.value = (unsigned long)&cnt;
		sys_bpf(BPF_MAP_UPDATE_ELEM, &attr);
	}
	close(fd);

	bzero(&attr, sizeof(attr));
	path = MES_MAP_START;
	attr.pathname = (unsigned long)path;
	attr.file_flags = BPF_F_WRONLY;
	fd = sys_bpf(BPF_OBJ_GET, &attr);

	for (idx = 0; idx < MES_DATA_SIZE; idx++) {
		bzero(&attr, sizeof(attr));
		attr.map_fd = fd;
		attr.key = (unsigned long)&idx;
		attr.value = (unsigned long)&data;
		sys_bpf(BPF_MAP_UPDATE_ELEM, &attr);
	}
	close(fd);

	bzero(&attr, sizeof(attr));
	path = MES_MAP_END;
	attr.pathname = (unsigned long)path;
	attr.file_flags = BPF_F_WRONLY;
	fd = sys_bpf(BPF_OBJ_GET, &attr);

	for (idx = 0; idx < MES_DATA_SIZE; idx++) {
		bzero(&attr, sizeof(attr));
		attr.map_fd = fd;
		attr.key = (unsigned long)&idx;
		attr.value = (unsigned long)&data;
		sys_bpf(BPF_MAP_UPDATE_ELEM, &attr);
	}
	close(fd);

	return 0;
}
