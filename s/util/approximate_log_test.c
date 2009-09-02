
#include "i.h"


int
main(int argc, char*argv[]) {
	testing_initialize(&argc, &argv, "");
	for (double x = 2.72; x < 1000; x+= 7.1)
		printf("log(%g) = %g approximation = %g\n", x, log(x), approximate_log(x));
	return 0;
}
