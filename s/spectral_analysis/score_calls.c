#include "i.h"

int
main(int argc, char *argv[]) {
	int optind = initialize(argc, argv, "<sound files>");
	for (int i = optind; i < argc; i++)
		score_calls_file(argv[i]);
	return 0;
}

