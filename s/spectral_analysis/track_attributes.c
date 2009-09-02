#include "i.h"

void
print_track_attributes(track_t *t, FILE *index_file, FILE *attribute_file) {
	int n_steps = t->points->len;
	assert(n_steps > 1);
	double sampling_rate = t->fft.sampling_rate;
	double seconds_per_step = t->fft.step_size/sampling_rate;
	double x[n_steps];
	double y[n_steps];
	int n = 0;
	for (int step = 0; step < n_steps; step++) {
		sinusoid_t s = g_array_index(t->points, sinusoid_t, step);
		x[n] = step*seconds_per_step;
		y[n] = s.frequency*sampling_rate;
		dp(21, "x[%d]=%g y[%d]=%g\n", n, x[n], n, y[n]);
		n++;
	}
	double c0, c1, cov00, cov01, cov11, sumsq;
	gsl_fit_linear(x, 1, y, 1, n, &c0, &c1, &cov00, &cov01, &cov11, &sumsq);
	if (index_file)
		fprintf(index_file, "length=%gs c0=%ghz c1=%ghz/s cov00=%g cov01=%g cov11=%g sumsq/n=%g\n", n_steps*seconds_per_step, c0, c1, cov00, cov01, cov11, sumsq/n_steps);
	if (attribute_file)
		fprintf(attribute_file, "length=%g\nc0=%g\nc1=%g\ncov00=%g\ncov01=%g\ncov11=%g\nsumsq=%g\n", n_steps*seconds_per_step, c0, c1, cov00, cov01, cov11, sumsq/n_steps);
//	slope = 1 + 0.5*c1/c0;
//	if (c0 < epsilon || slope < epsilon) {
//		// fit with first & last point - if fit on all points fails
//		c0 = y[0];
//		c1 = (y[n_steps - 1]-y[0])/((x[n_steps - 1] - x[0]));
//		slope = 1+0.5*c1/c0;
//	}
}
