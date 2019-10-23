/* SPDX-License-Identifier: GPL-2.0 */
#include "bpf_defs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <linux/version.h>
#include <gelf.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <assert.h>

#define MAX_MAPS 32
#define LOG_BUF_SIZE 40960

static char log_buf[LOG_BUF_SIZE];

static struct bpf_obj_info {
	struct bpf_insn *insns;
	int nr_insn;
} bpf_obj_info;

static int map_open(struct bpf_elf_map *map, char *map_name)
{
	union bpf_attr attr;
	int fd;
	char path[strlen(BPF_TC_GLOBAL) + 256];

	if (map->pinning) {
		if (map->pinning == PIN_TC_GLOBAL) {
			strcpy(path, BPF_TC_GLOBAL);
		} else {
			strcpy(path, BPF_FS);
		}
		assert(sizeof(path) > strlen(path) + strlen(map_name));
		strcat(path, map_name);

		bzero(&attr, sizeof(attr));
		attr.pathname = (unsigned long)path;
		fd = bpf_sys(BPF_OBJ_GET, &attr);
		if (fd >= 0) {
			/* OK. map exists. */
			return fd;
		}
		perror(map_name); /* just info message */
	}

	bzero(&attr, sizeof(attr));
	attr.map_type = map->type;
	attr.key_size = map->size_key;
	attr.value_size = map->size_value;
	attr.max_entries = map->max_elem;
	strncpy(attr.map_name, map_name, BPF_OBJ_NAME_LEN - 1);
	fd = bpf_sys(BPF_MAP_CREATE, &attr);
	if (fd < 0) {
		fprintf(stderr, "%s: ", map_name);
		perror("BPF_MAP_CRETE");
		return -1;
	}

	if (map->pinning) {
		bzero(&attr, sizeof(attr));
		attr.pathname = (unsigned long)path;
		attr.bpf_fd = fd;
		if (bpf_sys(BPF_OBJ_PIN, &attr) == -1) {
			perror("BPF_OBJ_PIN");
			close(fd);
			return -1;
		}
	}

	return fd;
}

static int get_bpf_obj_info(int ofd)
{
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
	int nr_rel = 0;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "elf_version: %s\n", elf_errmsg(-1));
		return -1;
	}

	elf = elf_begin(ofd, ELF_C_READ, (Elf *)NULL);
	if (!elf) {
		fprintf(stderr, "elf_begin: %s\n", elf_errmsg(-1));
		return -1;
	}

	if (gelf_getehdr(elf, &ehdr) == NULL) {
		fprintf(stderr, "gelf_getehdr: %s\n", elf_errmsg(-1));
		return -1;
	}

	for (i = 1; i < ehdr.e_shnum; i++) {
		scn = elf_getscn(elf, i);
		if (!scn) {
			fprintf(stderr, "elf_getscn(%d): %s\n", i, elf_errmsg(-1));
			return -1;
		}
		if (gelf_getshdr(scn, &shdr) != &shdr) {
			fprintf(stderr, "gelf_getshdr(%d): %s\n", i, elf_errmsg(-1));
			return -1;
		}
		name = elf_strptr(elf, ehdr.e_shstrndx, shdr.sh_name);
		if (!name) {
			fprintf(stderr, "elf_strptr(%d): %s\n", i, elf_errmsg(-1));
			return -1;
		}
		if (strcmp(name, "maps") == 0) {
			map_data = elf_getdata(scn, NULL);
			if (!map_data) {
				fprintf(stderr, "elf_getdata(maps): %s\n", elf_errmsg(-1));
				return -1;
			}
		} else if (strcmp(name, "prog") == 0) {
			prog_data = elf_getdata(scn, NULL);
			if (!prog_data) {
				fprintf(stderr, "elf_getdata(prog): %s\n", elf_errmsg(-1));
				return -1;
			}
		} else if (shdr.sh_type == SHT_REL) {
			nr_rel = shdr.sh_size / shdr.sh_entsize;
			rel_data = elf_getdata(scn, NULL);
			if (!rel_data) {
				fprintf(stderr, "elf_getdata(rel): %s\n", elf_errmsg(-1));
				return -1;
			}
		} else if (shdr.sh_type == SHT_SYMTAB) {
			sym_data = elf_getdata(scn, NULL);
			if (!sym_data) {
				fprintf(stderr, "elf_getdata(sym): %s\n", elf_errmsg(-1));
				return -1;
			}
		} else if (shdr.sh_type == SHT_STRTAB) {
			str_data = elf_getdata(scn, NULL);
			if (!str_data) {
				fprintf(stderr, "elf_getdata(str): %s\n", elf_errmsg(-1));
				return -1;
			}
		}
	}

	if (!prog_data) {
		fprintf(stderr, "no program\n");
		return -1;
	}

	bpf_obj_info.insns = (struct bpf_insn *)malloc(prog_data->d_size);
	if (bpf_obj_info.insns == NULL) {
		perror("malloc insns");
		return -1;
	}
	bpf_obj_info.nr_insn = prog_data->d_size / sizeof(struct bpf_insn);
	bcopy(prog_data->d_buf, bpf_obj_info.insns, prog_data->d_size);

	if (!map_data) {
		return 0;
	}

	/* handle maps */

	if (!rel_data || !sym_data || !str_data) {
		fprintf(stderr, "map related data not found\n");
		return -1;
	}

	nr_map = map_data->d_size / sizeof(struct bpf_elf_map);
	for (i = 0; i < nr_map; i++) {
		map_fds[i] = -1;
	}
	maps = (struct bpf_elf_map *)map_data->d_buf;

	for (i = 0; i < nr_rel; i++) {
		GElf_Rel rel;
		GElf_Sym sym;
		unsigned int insn_idx;
		unsigned int map_idx;

		if (gelf_getrel(rel_data, i, &rel) == NULL) {
			fprintf(stderr, "gelf_getrel(%d): %s\n", i, elf_errmsg(-1));
			return -1;
		}
		insn_idx = rel.r_offset / sizeof(struct bpf_insn);
		if (gelf_getsym(sym_data, GELF_R_SYM(rel.r_info), &sym) == NULL) {
			fprintf(stderr, "gelf_getsym(%d): %s\n", i, elf_errmsg(-1));
			return -1;
		}
		map_idx = sym.st_value / sizeof(struct bpf_elf_map);

		if (map_fds[map_idx] == -1) {
			map_fds[map_idx] = map_open(maps + map_idx,
						str_data->d_buf + sym.st_name);
			if (map_fds[map_idx] < 0) {
				return -1;
			}
		}

		bpf_obj_info.insns[insn_idx].src_reg = BPF_PSEUDO_MAP_FD;
		bpf_obj_info.insns[insn_idx].imm = map_fds[map_idx];
	}

	/* NOTE: map fds are expected to be closed at program termination. */

	return 0;
}

static int load_program(char *event)
{
	int prog_fd;
	union bpf_attr attr;
	char *license = "GPL";

	bzero(&attr, sizeof(attr));
	attr.prog_type = BPF_PROG_TYPE_KPROBE;
	attr.insns = (unsigned long)bpf_obj_info.insns;
	attr.insn_cnt = bpf_obj_info.nr_insn;
	/*attr.kern_version = LINUX_VERSION_CODE;*/
	attr.kern_version = 266001;
	attr.license = (unsigned long)license;
	attr.log_buf = (unsigned long)log_buf;
	attr.log_size = sizeof(log_buf);
	attr.log_level = 1;
	strncpy(attr.prog_name, event, BPF_OBJ_NAME_LEN - 1);
	prog_fd = bpf_sys(BPF_PROG_LOAD, &attr);
	if (prog_fd < 0) {
		perror("bpf_prog_load");
		fprintf(stderr, "%s\n", log_buf);
		return -1;
	}

	return prog_fd;
}

static int get_event_id(char *event)
{
	char buf[256];
	int fd;
	ssize_t ret;

	strcpy(buf, "/sys/kernel/debug/tracing/events/kprobes/");
	assert(strlen(buf) + strlen(event) + strlen("/id") < sizeof(buf));
	strcat(buf, event);
	strcat(buf, "/id");

	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		perror("open id");
		return -1;
	}

	ret = read(fd, buf, sizeof(buf) - 1);
	if (ret < 0) {
		perror("read id");
		close(fd);
		return -1;
	}
	close(fd);

	buf[ret] = 0;

	return atoi(buf);
}

static int set_event(int prog_fd, char *event, char *func)
{
	struct perf_event_attr ev_attr = {};
	int efd;
	char buf[256];
	int len;
	int id;

	len = snprintf(buf, sizeof(buf),
		"echo 'p:kprobes/%s %s' >> /sys/kernel/debug/tracing/kprobe_events",
		event, func);
	assert(len < sizeof(buf));
	if (system(buf) < 0) {
		perror("set event");
		return -1;
	}

	id = get_event_id(event);
	if (id == -1) {
		return -1;
	}

	ev_attr.config = id;
	ev_attr.type = PERF_TYPE_TRACEPOINT;

	efd = syscall(__NR_perf_event_open, &ev_attr,
		       -1/*pid*/, 0/*cpu*/, -1/*group_fd*/, 0);
	if (efd < 0) {
		perror("perf_event_open");
		return -1;
	}
	if (ioctl(efd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
		perror("PERF_EVENT_IOC_ENABLE");
		close(efd);
		return -1;
	}
	if (ioctl(efd, PERF_EVENT_IOC_SET_BPF, prog_fd) < 0) {
		perror("PERF_EVENT_IOC_SET_BPF");
		close(efd);
		return -1;
	}

	return efd;
}

void clear_event(char *event)
{
	char buf[256];

	snprintf(buf, sizeof(buf),
		"echo '-:kprobes/%s' >> /sys/kernel/debug/tracing/kprobe_events",
		event);
	if (system(buf) < 0) {
		perror("clear event");
	}
}

static void handle_sigint(int sig)
{
	printf("Terminate.\n");
}

int main(int argc, char *argv[])
{
	int ofd;
	char *func;
	char event[100];
	int prog_fd, efd;
	int len;

	if (argc != 3) {
		fprintf(stderr, "usage: load_kp_bpf object-file func-name\n");
		return 1;
	}
	func = argv[2];
	ofd = open(argv[1], O_RDONLY);
	if (ofd < 0) {
		perror("open object file failed");
		return 1;
	}

	if (get_bpf_obj_info(ofd) == -1) {
		close(ofd);
		return 1;
	}
	close(ofd);

	len = snprintf(event, sizeof(event), "%s_%d", func, (int)getpid());
	assert(len < sizeof(event));

	prog_fd = load_program(event);
	if (prog_fd < 0) {
		return 1;
	}

	efd = set_event(prog_fd, event, func);
	if (efd == -1) {
		close(prog_fd);
		return 1;
	}

	if (signal(SIGINT, handle_sigint) == SIG_ERR) {
		perror("signal");
		close(efd);
		close(prog_fd);
		return 1;
	}

	printf("Running.\n");
	pause();

	close(efd);
	close(prog_fd);
	clear_event(event);

	return 0;
}
