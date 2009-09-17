#include "i.h"

#define VERSION "0.1.1"

int
main(int argc, char *argv[]) {
	int optind = initialize(argc, argv, VERSION, "<sound files>");
	for (int i = optind; i < argc; i++)
		score_calls_file(argv[i]);
	return 0;
}

