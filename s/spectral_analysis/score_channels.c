#include "i.h"

#define VERSION "0.1.1"

int
main(int argc, char *argv[]) {
	int optind = initialize(argc, argv, SPECTRAL_ANALYSIS_GROUP, VERSION, "<sound files>");
	for (int i = optind; i < argc; i++)
		score_channels_file(argv[i]);
	return 0;
}

