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
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/mman.h>
#include <poll.h>
#include <sys/sysinfo.h>

#define MAX_CPU 64
#define PAGE_SIZE 4096
#define NR_DATA_PAGE 16 /* must be power of 2 */

static void
handle_signal(int sig)
{
	fprintf(stderr, "Terminate.\n");
}

struct perf_event_sample {
        struct perf_event_header header;
        __u32 size;
        char data[];
};

static void
output_event(int ofd, void *map_base)
{
	volatile struct perf_event_mmap_page *header = map_base;
	__u64 data_tail = header->data_tail;
	__u64 data_head = header->data_head;
	__u64 buffer_size = PAGE_SIZE * NR_DATA_PAGE;
	void *base, *begin, *end;
	char buf[256];

#if 0
	asm volatile("" ::: "memory"); /* in real code it should be smp_rmb() */
#endif
	if (data_head == data_tail)
		return;

        base = ((char *)header) + PAGE_SIZE;

        begin = base + data_tail % buffer_size;
        end = base + data_head % buffer_size;

        while (begin != end) {
                struct perf_event_sample *e;

                e = begin;
                if (begin + e->header.size > base + buffer_size) {
                        long len = base + buffer_size - begin;
                        memcpy(buf, begin, len);
                        memcpy(buf + len, base, e->header.size - len);
                        e = (void *) buf;
                        begin = base + e->header.size - len;
                } else if (begin + e->header.size == base + buffer_size) {
                        begin = base;
                } else {
                        begin += e->header.size;
                }

                if (e->header.type == PERF_RECORD_SAMPLE) {
			/* output size and data */
			if (write(ofd, &e->size, (size_t)e->size + sizeof(__u32)) == -1) {
				fprintf(stderr, "wirte failed\n");
			}
                } else if (e->header.type == PERF_RECORD_LOST) {
                        struct {
                                struct perf_event_header header;
                                __u64 id;
                                __u64 lost;
                        } *lost = (void *) e;
                        fprintf(stderr, "lost %lld events\n", lost->lost);
                } else {
                        fprintf(stderr, "unknown event type=%d size=%d\n",
                               e->header.type, e->header.size);
                }
        }

#if 0
	__sync_synchronize(); /* smp_mb() */
#endif
        header->data_tail = data_head;
}


static int map_open(char *map_path, int nr_cpu)
{
	char *map_name;
	union bpf_attr attr;
	int fd;

	map_name = strrchr(map_path, '/');
	if (map_name == NULL || *(map_name + 1) == 0) {
		fprintf(stderr, "Invalid map path: %s\n", map_path);
		return -1;
	}
	map_name++;

	bzero(&attr, sizeof(attr));
	attr.pathname = (unsigned long)map_path;
	fd = syscall(__NR_bpf, BPF_OBJ_GET, &attr, sizeof(attr));
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
	fd = syscall(__NR_bpf, BPF_MAP_CREATE, &attr, sizeof(attr));
	if (fd < 0) {
		perror("map create failed.");
		return -1;
	}

	bzero(&attr, sizeof(attr));
	attr.pathname = (unsigned long)map_path;
	attr.bpf_fd = fd;
	if (syscall(__NR_bpf, BPF_OBJ_PIN, &attr, sizeof(attr)) == -1) {
		perror("map pin failed.");
		close(fd);
		return -1;
	}

	return fd;
}

int main(int argc, char *argv[])
{
	char *map_path;
	char *map_name;
	int nr_cpu;
	char *endp;
	int ofd, map_fd;
	union bpf_attr attr;
	struct pollfd pfd[MAX_CPU];
	void *perf_header[MAX_CPU];
	struct perf_event_attr ev_attr;
	int i;

	if (argc != 3) {
		fprintf(stderr, "usage: bpf_eventd map-path output-file\n");
		return -1;
	}
	map_path = argv[1];

	nr_cpu = get_nprocs_conf();
	if (nr_cpu < 1 || nr_cpu > MAX_CPU) {
		fprintf(stderr, "Invalid cpu number: %d\n", nr_cpu);
		return -1;
	}
	fprintf(stderr, "number of cpu: %d\n", nr_cpu);

	if (strcmp(argv[2], "-") == 0) {
		ofd = fileno(stdout);
	} else {
		ofd = creat(argv[2], 0644);
		if (ofd < 0) {
			perror("open output file failed");
			return -1;
		}
	}

	map_fd = map_open(map_path, nr_cpu);
	if (map_fd < 0) {
		return -1;
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
		void *base;

		fd = syscall(__NR_perf_event_open, &ev_attr, -1, i, -1,
				PERF_FLAG_FD_CLOEXEC);
		if (fd < 0) {
			fprintf(stderr, "perf_event_open(%d) falied", i);
			return -1;
		}

		bzero(&attr, sizeof(attr));
		attr.map_fd = map_fd;
		attr.key = (unsigned long)&i;
		attr.value = (unsigned long)&fd;
		if (syscall(__NR_bpf, BPF_MAP_UPDATE_ELEM, &attr, sizeof(attr)) < 0) {
			fprintf(stderr, "update map(%d) falied", i);
			return -1;
		}

		if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
			fprintf(stderr, "event enable(%d) falied", i);
			return -1;
		}

		base = mmap(NULL, PAGE_SIZE * (NR_DATA_PAGE + 1),
			       	PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (base == MAP_FAILED) {
			fprintf(stderr, "mmap(%d) falied", i);
			return -1;
		}

		pfd[i].fd = fd;
		pfd[i].events = POLLIN;
		perf_header[i] = base;
	}

	if (signal(SIGINT, handle_signal) == SIG_ERR) {
		perror("signal");
		return -1;
	}
	if (signal(SIGTERM, handle_signal) == SIG_ERR) {
		perror("signal");
		return -1;
	}

	fprintf(stderr, "Running.\n");
	for (;;) {
		int ret;
		int err = 0;
		ret = poll(pfd, nr_cpu, -1);
		if (ret < 0) {
			perror("poll");
			break;
		}
		for (i = 0; i < nr_cpu; i++) {
			if (pfd[i].revents & (POLLERR | POLLHUP)) {
				err = 1;
				break;
			}
			output_event(ofd, perf_header[i]);
		}
		if (err) {
			break;
		}
	}

	/* close files via exit. */
	return 0;
}
