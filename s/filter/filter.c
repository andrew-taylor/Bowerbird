#include "i.h"
#define VERSION "0.1"

int
main(int argc, char *argv[]) {
	(void)initialize(argc, argv, FILTER_GROUP, VERSION, "<sound files>");
	comb("/dev/stdin", "/dev/stdout");
	return 0;
}

