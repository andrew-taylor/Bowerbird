
#include "i.h"

double 
approximate_log(double x) {
	uint32_t i =  x*65536;
	int y = 0;
	while (i >= 2*65536) {
		i >>= 1;
		y++;
	}
	x = i/65536.0;
	x--;
	return (0.693147180559945*y+ x*(6 + 0.7662*x)/(5.9897 + 3.7658*x));
}
