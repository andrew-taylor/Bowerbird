#include "comb_filter.h"

static void usage(void) {
	fprintf(stderr, "Usage %s [-V] [-v verbosity] <commands> \n", myname);
}

char *version = "0.1";

static char *short_options = "o:v:V";
static struct option long_options[] = {
	{"option", 1, 0, 'o'},
	{"verbosity", 1, 0, 'v'},
	{"version", 0, 0, 'V'},
	{0, 0, 0, 0}
};

int main(int argc, char *argv[]) {
	verbosity = 1;
	debug_stream = stderr;
	set_myname(argv);
	optind = 0;
	initialize_options();
	while (1) {
		int option_index;
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (c == -1)
			break;
		opterr = 0;
		switch (c) {
		case 'o':
			parse_option_assignment(optarg);
 			break;
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
	postprocess_options();
	comb("/dev/stdin", "/dev/stdout");
	return 0;
}
