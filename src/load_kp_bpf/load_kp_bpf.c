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
#include <linux/version.h>
#include <gelf.h>
#include "bpf_elf.h"
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <signal.h>

#define MAX_MAPS 32

static int map_open(struct bpf_elf_map *map, char *map_name)
{
	union bpf_attr attr;
	int fd;
	char path[256];

	if (map->pinning) {
		if (map->pinning == 2) {
			/* pinning == 2 means using tc map */
			strcpy(path, "/sys/fs/bpf/tc/globals/");
		} else {
			strcpy(path, "/sys/fs/bpf/");
		}
		strcat(path, map_name);

		bzero(&attr, sizeof(attr));
		attr.pathname = (unsigned long)path;
		fd = syscall(__NR_bpf, BPF_OBJ_GET, &attr, sizeof(attr));
		if (fd >= 0) {
			/* OK. map exists. */
			return fd;
		}
		perror(map_name);
	}

	bzero(&attr, sizeof(attr));
	attr.map_type = map->type;
	attr.key_size = map->size_key;
	attr.value_size = map->size_value;
	attr.max_entries = map->max_elem;
	strncpy(attr.map_name, map_name, BPF_OBJ_NAME_LEN - 1);
	fd = syscall(__NR_bpf, BPF_MAP_CREATE, &attr, sizeof(attr));
	if (fd < 0) {
		return -1;
	}

	if (map->pinning) {
		bzero(&attr, sizeof(attr));
		attr.pathname = (unsigned long)path;
		attr.bpf_fd = fd;
		if (syscall(__NR_bpf, BPF_OBJ_PIN, &attr, sizeof(attr)) == -1) {
			perror("obj pin");
			return -1;
		}
	}

	return fd;
}

static void
handle_sigint(int sig)
{
	printf("Terminate.\n");
}

static char log_buf[40960];

int main(int argc, char *argv[])
{
	int ofd;
	char *func;
	char event[100];
	Elf *elf;
	GElf_Ehdr ehdr;
	int i;
	Elf_Scn *scn;
	GElf_Shdr shdr;
	char *name;
	Elf_Data *prog_data = NULL, *map_data = NULL;
	Elf_Data *sym_data = NULL, *rel_data = NULL, *str_data = NULL;
	int nr_map = 0;
	struct bpf_elf_map *maps;
	int map_fds[MAX_MAPS];
	struct bpf_insn *insns;
	int nr_insn;
	int maps_shndx;
	int nr_rel;
	union bpf_attr attr;
	int prog_fd;
	char *license = "GPL";
	struct perf_event_attr ev_attr = {};
	int efd;
	char buf[256];
	ssize_t ret;

	if (argc != 3) {
		printf("usage: load_kp_bpf object-file event-name\n");
		return -1;
	}
	func = argv[2];
	ofd = open(argv[1], O_RDONLY);
	if (ofd < 0) {
		perror("open object file failed");
		return -1;
	}

	if (elf_version(EV_CURRENT) == EV_NONE) {
		printf("elf_version: %s\n", elf_errmsg(-1));
		return -1;
	}

	elf = elf_begin(ofd, ELF_C_READ, (Elf *)NULL);
	if (!elf) {
		printf("elf_begin: %s\n", elf_errmsg(-1));
		return -1;
	}

	if (gelf_getehdr(elf, &ehdr) == NULL) {
		printf("gelf_getehdr: %s\n", elf_errmsg(-1));
		return -1;
	}

	for (i = 1; i < ehdr.e_shnum; i++) {
		scn = elf_getscn(elf, i);
		if (!scn) {
			printf("elf_getscn: %s\n", elf_errmsg(-1));
			return -1;
		}
		if (gelf_getshdr(scn, &shdr) != &shdr) {
			printf("gelf_getshdr: %s\n", elf_errmsg(-1));
			return -1;
		}
		name = elf_strptr(elf, ehdr.e_shstrndx, shdr.sh_name);
		if (!name) {
			printf("gelf_getshdr: %s\n", elf_errmsg(-1));
			return -1;
		}
		if (strcmp(name, "maps") == 0) {
			maps_shndx = i;
			map_data = elf_getdata(scn, NULL);
			if (!map_data) {
				printf("elf_getdata: %s\n", elf_errmsg(-1));
				return -1;
			}
		} else if (strcmp(name, "prog") == 0) {
			prog_data = elf_getdata(scn, NULL);
			if (!prog_data) {
				printf("elf_getdata: %s\n", elf_errmsg(-1));
				return -1;
			}
		} else if (shdr.sh_type == SHT_REL) {
			nr_rel = shdr.sh_size / shdr.sh_entsize;
			rel_data = elf_getdata(scn, NULL);
			if (!rel_data) {
				printf("elf_getdata: %s\n", elf_errmsg(-1));
				return -1;
			}
		} else if (shdr.sh_type == SHT_SYMTAB) {
			sym_data = elf_getdata(scn, NULL);
			if (!sym_data) {
				printf("elf_getdata: %s\n", elf_errmsg(-1));
				return -1;
			}
		} else if (shdr.sh_type == SHT_STRTAB) {
			str_data = elf_getdata(scn, NULL);
			if (!str_data) {
				printf("elf_getdata: %s\n", elf_errmsg(-1));
				return -1;
			}
		}
	}

	if (!prog_data) {
		printf("no program\n");
		return -1;
	}
	nr_insn = prog_data->d_size / sizeof(struct bpf_insn);
	insns = (struct bpf_insn *)prog_data->d_buf;

	if (map_data) {
		if (!rel_data || !sym_data || !str_data) {
			printf("map related data not found\n");
			return -1;
		}
		nr_map = map_data->d_size / sizeof(struct bpf_elf_map);
		maps = (struct bpf_elf_map *)map_data->d_buf;

		for (i = 0; i < nr_map; i++) {
			map_fds[i] = -1;
		}

		for (i = 0; i < nr_rel; i++) {
			GElf_Rel rel;
			GElf_Sym sym;
			unsigned int insn_idx;
			unsigned int map_idx;

			gelf_getrel(rel_data, i, &rel);
			insn_idx = rel.r_offset / sizeof(struct bpf_insn);
			gelf_getsym(sym_data, GELF_R_SYM(rel.r_info), &sym);
			map_idx = sym.st_value / sizeof(struct bpf_elf_map);

			if (map_fds[map_idx] == -1) {
				map_fds[map_idx] = map_open(maps + map_idx, str_data->d_buf + sym.st_name);
				if (map_fds[map_idx] < 0) {
					perror("map_open");
					return -1;
				}
			}

			insns[insn_idx].src_reg = BPF_PSEUDO_MAP_FD;
			insns[insn_idx].imm = map_fds[map_idx];
		}
	}

	snprintf(event, sizeof(event), "%s_%d", func, (int)getpid());

	bzero(&attr, sizeof(attr));
	attr.prog_type = BPF_PROG_TYPE_KPROBE;
	attr.insns = (unsigned long)insns;
	attr.insn_cnt = nr_insn;
	attr.kern_version = LINUX_VERSION_CODE;
	attr.license = (unsigned long)license;
	attr.log_buf = (unsigned long)log_buf;
	attr.log_size = sizeof(log_buf);
	attr.log_level = 1;
	strncpy(attr.prog_name, event, BPF_OBJ_NAME_LEN - 1);
	prog_fd = syscall(__NR_bpf, BPF_PROG_LOAD, &attr, sizeof(attr));
	if (prog_fd < 0) {
		perror("bpf_prog_load error");
		printf("%s\n", log_buf);
		return -1;
	}

	snprintf(buf, sizeof(buf),
		"echo 'p:kprobes/%s %s' >> /sys/kernel/debug/tracing/kprobe_events",
		event, func);
	if (system(buf) < 0) {
		perror("system");
		return -1;
	}

	strcpy(buf, "/sys/kernel/debug/tracing/events/kprobes/");
	strcat(buf, event);
	strcat(buf, "/id");
	efd = open(buf, O_RDONLY);
	if (efd < 0) {
		perror("open id");
		return -1;
	}
	ret = read(efd, buf, sizeof(buf));
	if (ret < 0) {
		perror("read id");
		return -1;
	}
	close(efd);

	buf[ret] = 0;
	ev_attr.config = atoi(buf);
	ev_attr.type = PERF_TYPE_TRACEPOINT;

	efd = syscall(__NR_perf_event_open, &ev_attr, -1/*pid*/, 0/*cpu*/, -1/*group_fd*/, 0);
	if (efd < 0) {
		perror("perf_event_open");
		return -1;
	}
	if (ioctl(efd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
		perror("PERF_EVENT_IOC_ENABLE");
		return -1;
	}
	if (ioctl(efd, PERF_EVENT_IOC_SET_BPF, prog_fd) < 0) {
		perror("PERF_EVENT_IOC_SET_BPF");
		return -1;
	}

	if (signal(SIGINT, handle_sigint) == SIG_ERR) {
		perror("signal");
		return -1;
	}

	printf("Running.\n");
	pause();

	close(efd);
	snprintf(buf, sizeof(buf),
		"echo -:kprobes/%s >> /sys/kernel/debug/tracing/kprobe_events",
		event);
	system(buf);

	return 0;
}
