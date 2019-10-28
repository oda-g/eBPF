/* SPDX-License-Identifier: GPL-2.0 */
#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <signal.h>
#include <poll.h>
#include <linux/perf_event.h>
#include <errno.h>
#include <string.h>

#define PAGE_SIZE 4096
#define NR_DATA_PAGE 4 /* must be power of 2 */

static void
handle_signal(int sig)
{
	fprintf(stderr, "Terminate.\n");
}

struct perf_event_sample {
        struct perf_event_header header;
        uint32_t pid, tid;
        uint64_t time;
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
			printf("pid: %u, tid: %u, time: %lu\n",
				e->pid, e->tid, e->time);
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

int main(int argc, char *argv[])
{
	int nr_cpu;
	int cpu;
	char *endp;
	int fd;
	struct pollfd pfd;
	void *perf_header;
	struct perf_event_attr ev_attr;

	if (argc != 2) {
		fprintf(stderr, "usage: cs_eventd CPU\n");
		return 1;
	}

	nr_cpu = get_nprocs_conf();

	cpu = strtol(argv[1], &endp, 10);
	if ('\0' != *endp || cpu >= nr_cpu) {
		fprintf(stderr, "invalid cpu number: %s\n", argv[1]);
		return 1;
	}

	/* NOTE: This function will return without freeing resources
	 * (open files, mapping memory).
	 * They are expected to free at the process exit.
	 */

	bzero(&ev_attr, sizeof(ev_attr));
	ev_attr.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_TIME;
	ev_attr.type = PERF_TYPE_SOFTWARE;
	ev_attr.config = PERF_COUNT_SW_CONTEXT_SWITCHES;
	ev_attr.sample_period = 1;
	ev_attr.wakeup_events = 1;

	fd = syscall(__NR_perf_event_open, &ev_attr, -1, cpu, -1,
			PERF_FLAG_FD_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "perf_event_open falied");
		return 1;
	}

	if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
		fprintf(stderr, "event enable falied");
		return 1;
	}

	perf_header = mmap(NULL, PAGE_SIZE * (NR_DATA_PAGE + 1),
			PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (perf_header == MAP_FAILED) {
		fprintf(stderr, "mmap falied");
		return 1;
	}

	if (signal(SIGINT, handle_signal) == SIG_ERR) {
		perror("signal");
		return 1;
	}

	fprintf(stderr, "Running.\n");

	pfd.fd = fd;
	pfd.events = POLLIN;
	for (;;) {
		if (poll(&pfd, 1, -1) == -1) {
			perror("poll");
			return 1;
		}
		if (pfd.revents | POLLIN) {
			output_event(perf_header);
		} else if (pfd.revents != 0) {
			fprintf(stderr, "poll error revents(%d)\n",
					pfd.revents);
			return 1;
		}
	}

	return 0;
}
