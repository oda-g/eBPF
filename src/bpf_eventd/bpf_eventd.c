/* SPDX-License-Identifier: GPL-2.0 */
#include "bpf_defs.h"
#include <stdio.h>
#include <sys/sysinfo.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <signal.h>
#include <poll.h>
#include <linux/perf_event.h>
#include <errno.h>
#include <string.h>
#include "bpf_eventd.h"

#define MAX_CPU 32
#define PAGE_SIZE 4096
#define NR_DATA_PAGE 4 /* must be power of 2 */

#define MAP_PATH BPF_FS MAP_NAME

static void
handle_signal(int sig)
{
	fprintf(stderr, "Terminate.\n");
}

struct perf_event_sample {
        struct perf_event_header header;
        uint32_t size;
	char data[];
};

struct perf_event_lost {
	struct perf_event_header header;
	uint64_t id;
	uint64_t lost;
};

static void
output_event(void *map_base)
{
	volatile struct perf_event_mmap_page *header = map_base;
	uint64_t data_tail = header->data_tail;
	uint64_t data_head = header->data_head;
	uint64_t buffer_size = PAGE_SIZE * NR_DATA_PAGE;
	void *base, *begin, *end;
	char buf[256];

	asm volatile("" ::: "memory"); /* smp_rmb() for x86_64 */

	if (data_head == data_tail) {
		return;
	}

        base = ((char *)header) + PAGE_SIZE;

        begin = base + data_tail % buffer_size;
        end = base + data_head % buffer_size;

        while (begin != end) {
                struct perf_event_header *e_hdr;

                e_hdr = begin;
                if (begin + e_hdr->size > base + buffer_size) {
                        long len = base + buffer_size - begin;
                        memcpy(buf, begin, len);
                        memcpy(buf + len, base, e_hdr->size - len);
                        e_hdr = (void *)buf;
                        begin = base + e_hdr->size - len;
                } else if (begin + e_hdr->size == base + buffer_size) {
                        begin = base;
                } else {
                        begin += e_hdr->size;
                }

                if (e_hdr->type == PERF_RECORD_SAMPLE) {
			struct perf_event_sample *e = (void *)e_hdr;
			struct bpf_sys_data *d = (void *)e->data;
			printf("cmd: %u, cpu: %u, time: %lu\n",
				d->cmd, d->cpu, d->time);	
                } else if (e_hdr->type == PERF_RECORD_LOST) {
			struct perf_event_lost *lost = (void *)e_hdr;
                        fprintf(stderr, "lost %lu events\n", lost->lost);
                } else {
                        fprintf(stderr, "unknown event type=%d size=%d\n",
                               e_hdr->type, e_hdr->size);
                }
        }

	__sync_synchronize(); /* smp_mb() */

        header->data_tail = data_head;
}

static int map_open(int nr_cpu)
{
	char *map_path = MAP_PATH;
	char *map_name = MAP_NAME;
	union bpf_attr attr;
	int fd;

	bzero(&attr, sizeof(attr));
	attr.pathname = (unsigned long)map_path;
	fd = bpf_sys(BPF_OBJ_GET, &attr);
	if (fd >= 0) {
		fprintf(stderr, "Warning: Map exists.\n");
		return fd;
	}

	bzero(&attr, sizeof(attr));
	attr.map_type = BPF_MAP_TYPE_PERF_EVENT_ARRAY;
	attr.key_size = 4;
	attr.value_size = 4;
	attr.max_entries = nr_cpu;
	strncpy(attr.map_name, map_name, BPF_OBJ_NAME_LEN - 1);
	fd = bpf_sys(BPF_MAP_CREATE, &attr);
	if (fd < 0) {
		perror("map create failed.");
		return -1;
	}

	bzero(&attr, sizeof(attr));
	attr.pathname = (unsigned long)map_path;
	attr.bpf_fd = fd;
	if (bpf_sys(BPF_OBJ_PIN, &attr) == -1) {
		perror("map pin failed.");
		close(fd);
		return -1;
	}

	return fd;
}

int main(int argc, char *argv[])
{
	int nr_cpu;
	int map_fd;
	struct pollfd pfd[MAX_CPU];
	struct perf_event_attr ev_attr;
	void *perf_header[MAX_CPU];
	int i;

	nr_cpu = get_nprocs_conf();
	if (nr_cpu < 1 || nr_cpu > MAX_CPU) {
		fprintf(stderr, "Invalid cpu number: %d\n", nr_cpu);
		return 1;
	}
	fprintf(stderr, "number of cpu: %d\n", nr_cpu);

	/* NOTE: open files will not be closed in this function.
	 * They are expected to close at the process exit.
	 */

	map_fd = map_open(nr_cpu);
	if (map_fd < 0) {
		return 1;
	}

	bzero(pfd, sizeof(pfd));
	bzero(&ev_attr, sizeof(ev_attr));
	ev_attr.sample_type = PERF_SAMPLE_RAW;
	ev_attr.type = PERF_TYPE_SOFTWARE;
	ev_attr.config = PERF_COUNT_SW_BPF_OUTPUT;
	ev_attr.sample_period = 1;
	ev_attr.wakeup_events = 1;
	for (i = 0; i < nr_cpu; i++) {
		int fd;
		union bpf_attr attr;
		void *base;

		fd = syscall(__NR_perf_event_open, &ev_attr, -1, i, -1,
				PERF_FLAG_FD_CLOEXEC);
		if (fd < 0) {
			fprintf(stderr, "perf_event_open(%d) falied: %s\n",
				       i, strerror(errno));
			return 1;
		}

		bzero(&attr, sizeof(attr));
		attr.map_fd = map_fd;
		attr.key = (unsigned long)&i;
		attr.value = (unsigned long)&fd;
		if (bpf_sys(BPF_MAP_UPDATE_ELEM, &attr) < 0) {
			fprintf(stderr, "update map(%d) falied: %s\n",
				       i, strerror(errno));
			return 1;
		}

		if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
			fprintf(stderr, "event enable(%d) falied: %s\n",
				       i, strerror(errno));
			return 1;
		}

		base = mmap(NULL, PAGE_SIZE * (NR_DATA_PAGE + 1),
			       	PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (base == MAP_FAILED) {
			fprintf(stderr, "mmap(%d) falied: %s\n",
				       i, strerror(errno));
			return 1;
		}

		pfd[i].fd = fd;
		pfd[i].events = POLLIN;
		perf_header[i] = base;
	}

	if (signal(SIGINT, handle_signal) == SIG_ERR) {
		perror("signal");
		return 1;
	}

	fprintf(stderr, "Running.\n");
	for (;;) {
		if (poll(pfd, nr_cpu, -1) == -1) {
			perror("poll");
			return 1;
		}
		for (i = 0; i < nr_cpu; i++) {
			if (pfd[i].revents | POLLIN) {
				output_event(perf_header[i]);
			} else if (pfd[i].revents != 0) {
				fprintf(stderr, "poll(%d) error revents(%d)\n",
						i, pfd[i].revents);
				return 1;
			}
		}
	}

	return 0;
}
