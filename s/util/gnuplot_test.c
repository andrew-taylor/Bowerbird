#include "i.h"

#include <math.h>

int
main(void) {
	verbosity = 30;
	int n = 100;
	double a[n], b[n], c[n], d[n], e[n];
	int g[n];
	int i;
	for (i = 0; i < n; i++) {
		a[i] = sin(2*M_PI*i/30);
		b[i] = cos(2*M_PI*i/30);
		c[i] = sin(2*M_PI*i/20);
		d[i] = cos(2*M_PI*i/20);
		e[i] = i;
		g[i] = i;
	}
	gnuplotf("'set logscale x' length=%d %d", n, g);
	gnuplotf("length=%d rows=2 title='a' %t%d%lf; title='b' %t%d%lf", n, g, a, g, b);
	gnuplotf("width=2000 height=1000 rows=3 length=%d %lf%lf;%lf%lf; with='points' %d", n, a, b,c,d,g);
	gnuplotf("width=2000 height=1000 rows=4 length=%d %lf;%lf;%t%lf%lf; with='points' title=%s %t%d%lf",  n, a, b, e, a, "using points", g, b);
	return 0;
}
