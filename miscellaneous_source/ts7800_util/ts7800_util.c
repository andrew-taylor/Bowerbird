#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <getopt.h>

char *version = "0.1";
int		verbosity = 0;
static char *short_options = "v:V";
static struct option long_options[] = {
	{"verbosity", 1, 0, 'v'},
	{"version", 0, 0, 'V'},
	{0, 0, 0, 0}
};
static void set_myname(char *argv[]);
static char *myname;

void
set_myname(char *argv[]) {
	myname = strrchr(argv[0], '/');
	if (myname == NULL)
		myname = argv[0];
	else
		myname++;
}

void
die(char *format, ...) {
	va_list ap;
	if (myname)
		fprintf(stderr, "%s: ", myname);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	if (errno == 0)
		fprintf(stderr, "\n");
	else
		perror(" ");
	exit(1);
}

FILE	*debug_stream;

void
dprintf(int level, char *format, ...) {
	va_list ap;
	if (level > verbosity)
		return;
	va_start(ap, format);
	if (debug_stream == NULL)
		vfprintf(stderr, format, ap);
	else
		vfprintf(debug_stream, format, ap);
}

static void usage(void) {
	fprintf(stderr, "Usage %s [-V] [-v verbosity] <commands> \n", myname);
}


int main(int argc, char *argv[]) {
	set_myname(argv);
	if (argc < 2)
		usage();
	optind = 0;
	while (1) {
		int option_index;
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (c == -1)
			break;
		dprintf(29, "c='%c'\n", c);
		opterr = 0;
		switch (c) {
		case 'v':
			verbosity = atoi(optarg);
  			break;
		case 'V':
			printf("%s v%s\n",myname, version);
  			exit(0);
		case '?':
			usage();
 		}
	}
	if (optind != argc - 2)
		die("Usage <bit> <value>\n");
	int fd = open("/dev/mem", O_RDWR|O_SYNC);
	uint8_t *start = mmap(0, getpagesize(), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0xE8000000);   
	volatile uint16_t *PBDR = (uint16_t *)(start + 0x08); //port b      
	volatile uint16_t *PBDDR = (uint16_t *)(start + 0x14); //port b direction
	*PBDDR = 0xffff;
	int bit = atoi(argv[optind]);
	dprintf(10, "bit = %d\n", bit);
	uint32_t val = *PBDR;
	if (atoi(argv[optind+1])) {
		val |= (1 << bit);
	} else {
		val &= ~(1 << bit);
	}
	dprintf(10, "*%p = %x\n", PBDR, val);
	*PBDR = val;
	close(fd);
	return 0;
}
