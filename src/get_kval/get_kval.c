#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <gelf.h>

int main(int argc, char *argv[])
{
	unsigned long addr;
	int fd;
	Elf *elf;
	GElf_Ehdr ehdr;
	GElf_Phdr phdr;
	int i;
	off_t offset = 0;
	unsigned long value;

	if (argc != 2) {
		printf("usage: get_kval address\n");
		return -1;
	}

	/* warning: no error check */
	addr = strtoul(argv[1], NULL, 0);
	fprintf(stderr, "addr: %lx\n", addr);

	fd = open("/proc/kcore", O_RDONLY);
	if (fd < 0) {
		perror("open /proc/kcore failed");
		return -1;
	}

	if (elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "elf_version: %s\n", elf_errmsg(-1));
		return -1;
	}

	elf = elf_begin(fd, ELF_C_READ, (Elf *)NULL);
	if (!elf) {
		fprintf(stderr, "elf_begin: %s\n", elf_errmsg(-1));
		return -1;
	}

	if (gelf_getehdr(elf, &ehdr) == NULL) {
		fprintf(stderr, "gelf_getehdr: %s\n", elf_errmsg(-1));
		return -1;
	}

	for (i = 1; i < ehdr.e_phnum; i++) {
		if (gelf_getphdr(elf, i, &phdr) != &phdr) {
			fprintf(stderr, "gelf_getphdr: %s\n", elf_errmsg(-1));
			return -1;
		}
		if (phdr.p_vaddr <= addr && addr < phdr.p_vaddr + phdr.p_memsz) {
			offset = phdr.p_offset + (addr - phdr.p_vaddr);
			break;
		}
	}

	if (offset == 0) {
		fprintf(stderr, "addr %lx is not kernel range\n", addr);
		return -1;
	}
	fprintf(stderr, "offset: %lu\n", offset);

	if (lseek(fd, offset, SEEK_SET) < 0) {
		perror("lseek");
		return -1;
	}

	if (read(fd, &value, sizeof(value)) != (ssize_t)sizeof(value)) {
		perror("read");
		return -1;
	}

	printf("%ld\n", value);

	return 0;
}
