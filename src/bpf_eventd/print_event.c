#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

int main(int argc, char *argv[])
{
	int ifd = fileno(stdin);
	struct {
		short idx;
		short cmd;
		uint32_t cpu;
		uint64_t time;
	} *data;
	ssize_t ret;
	uint32_t size;
	char buf[sizeof(*data) + 32];

	while (1) {
		ret = read(ifd, &size, sizeof(size));
		if (ret < 0) {
			perror("read");
			break;
		}

		ret = read(ifd, &buf, (size_t)size);
		if (ret < 0) {
			perror("read");
			break;
		}
		data = (void *)buf;
		printf("idx: %d, cmd: %d, cpu: %u, time: %lu\n",
		       (int)data->idx, (int)data->cmd, data->cpu, data->time);	
	}

	return 0;
}
