/*
 * filefrag.c -- report if a particular file is fragmented
 * 
 * Copyright 2003 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/ioctl.h>
#include <linux/fd.h>

int verbose = 0;

#define FIBMAP	   _IO(0x00,1)	/* bmap access */
#define FIGETBSZ   _IO(0x00,2)	/* get the block size used for bmap */

static unsigned long get_bmap(int fd, unsigned long block)
{
	int	ret;
	unsigned long b;

	b = block;
	ret = ioctl(fd, FIBMAP, &b);
	if (ret < 0) {
		if (errno == EPERM) {
			fprintf(stderr, "No permission to use FIBMAP ioctl; must have root privileges\n");
			exit(1);
		}
		perror("FIBMAP");
	}
	return b;
}

#define EXT2_DIRECT	12

void frag_report(const char *filename)
{
	struct statfs	fsinfo;
	struct stat64	fileinfo;
	long		i, fd, bs, block, last_block, numblocks;
	long		bpib;	/* Blocks per indirect block */
	long		cylgroups;
	int		discont = 0, expected;
	int		is_ext2 = 0;

	if (statfs(filename, &fsinfo) < 0) {
		perror("statfs");
		return;
	}
	if (stat64(filename, &fileinfo) < 0) {
		perror("stat");
		return;
	}
	if (!S_ISREG(fileinfo.st_mode)) {
		printf("%s: Not a regular file\n", filename);
		return;
	}
	if ((fsinfo.f_type == 0xef51) || (fsinfo.f_type == 0xef52) || 
	    (fsinfo.f_type == 0xef53))
		is_ext2++;
	if (verbose) {
		printf("Filesystem type is: %x\n", fsinfo.f_type);
	}
	cylgroups = (fsinfo.f_blocks + fsinfo.f_bsize*8-1) / fsinfo.f_bsize*8;
	if (verbose) {
		printf("Filesystem cylinder groups is approximately %ld\n", 
		       cylgroups);
	}
	fd = open(filename, O_RDONLY | O_LARGEFILE);
	if (fd < 0) {
		perror("open");
		return;
	}
	if (ioctl(fd, FIGETBSZ, &bs) < 0) {
		perror("FIGETBSZ");
		return;
	}
	if (verbose)
		printf("Blocksize of file %s is %ld\n", filename, bs);
	bpib = bs / 4;
	numblocks = (fileinfo.st_size + (bs-1)) / bs;
	if (verbose)
		printf("File size of %s is %lld (%ld blocks)\n", filename, 
		       (long long) fileinfo.st_size, numblocks);
	for (i=0; i < numblocks; i++) {
		if (is_ext2) {
			if (((i-EXT2_DIRECT) % bpib) == 0)
				last_block++;
			if (((i-EXT2_DIRECT-bpib) % (bpib*bpib)) == 0)
				last_block++;
			if (((i-EXT2_DIRECT-bpib-bpib*bpib) % (bpib*bpib*bpib)) == 0)
				last_block++;
		}
		block = get_bmap(fd, i);
		if (i && (block != last_block +1) ) {
			if (verbose)
				printf("Discontinuity: Block %ld is at %ld (was %ld)\n",
				       i, block, last_block);
			discont++;
		}
		if (block)
			last_block = block;
	}
	if (discont==0)
		printf("%s: 1 extent found", filename);
	else
		printf("%s: %d extents found", filename, discont+1);
	expected = (numblocks/((bs*8)-(fsinfo.f_files/8/cylgroups)-3))+1;
	if (is_ext2 && expected != discont+1)
		printf(", perfection would be %d extent%s\n", expected,
			(expected>1) ? "s" : "");
	else
		fputc('\n', stdout);
	
}

void usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [-v] file ...\n", progname);
	exit(1);
}

int main(int argc, char**argv)
{
	char **cpp;
	int c;

	while ((c = getopt(argc, argv, "v")) != EOF)
		switch (c) {
		case 'v':
			verbose++;
			break;
		default:
			usage(argv[0]);
			break;
		}
	if (optind == argc)
		usage(argv[0]);
	for (cpp=argv+optind; *cpp; cpp++) {
		if (verbose)
			printf("Checking %s\n", *cpp);
		frag_report(*cpp);
	}
	return 0;
}


	