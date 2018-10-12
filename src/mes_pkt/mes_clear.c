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
	unsigned long data = 0;
	int idx;

	bzero(&attr, sizeof(attr));
	attr.pathname = (unsigned long)path;
	attr.file_flags = BPF_F_WRONLY;
	fd = sys_bpf(BPF_OBJ_GET, &attr);

	for (idx = 0; idx < MES_ARRAY_SIZE; idx++) {
		bzero(&attr, sizeof(attr));
		attr.map_fd = fd;
		attr.key = (unsigned long)&idx;
		attr.value = (unsigned long)&data;
		sys_bpf(BPF_MAP_UPDATE_ELEM, &attr);
	}

	return 0;
}
