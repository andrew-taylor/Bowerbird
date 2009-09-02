#include "i.h"

// FIXME remove ugly define
#define MAX_TESTS 1000000
static int n_tests = 0;
static sinusoid_t errors[MAX_TESTS];

#ifdef POWER_T_32
#define MAXIMUM_AMPLITUDE_ERROR 1
#define MAXIMUM_FREQUENCY_ERROR 0.04
#else
#ifdef POWER_T_DOUBLE
#define MAXIMUM_AMPLITUDE_ERROR 0.1
#define MAXIMUM_FREQUENCY_ERROR 0.01
#else
#define MAXIMUM_AMPLITUDE_ERROR 0.2
#define MAXIMUM_FREQUENCY_ERROR 0.02
#endif
#endif

static void single_test(int fft_length, sinusoid_t s) {
	// ignore sinusoids much smaller than the sinusoid being searched for
	param_set_double("spectral_analysis", "peak_min_height", 0.001*s.amplitude);
	param_set_double("spectral_analysis", "fft_points", fft_length);
	dp(27, "min_height=%s\n", param_get_string("spectral_analysis", "peak_min_height"));
	int n_samples = fft_length;
	sample_t samples[n_samples];
	int frame_length = fft_length;
	int n_bins = (fft_length + 1)/2;
	phase_t phase[n_bins];
	dp(11, "frequency=%g amplitude=%g phase=%g fft_length=%d\n", s.frequency, s.amplitude, s.phase, fft_length);
	int max_sinusoids = fft_length;
	sinusoid_t estimated[max_sinusoids];
	set_sinusoid(samples, n_samples, s);
	// gnuplotf("length=%d title='a' %lf", n_samples, samples);
	power_t power[n_bins];
	extract_power_phase(samples, frame_length, fft_length, 1, power, phase);
	int n_sinusoids;
	n_sinusoids = find_sinusoids(n_bins, power, phase, 0, 0, estimated);
	if (n_sinusoids != 1) {
		for (int i = 0; i < n_sinusoids; i++)
			dp(1, "sinusoid[%d] frequency=%g amplitude=%g phase=%g\n", i, estimated[i].frequency, estimated[i].amplitude, estimated[i].phase);
		die("n_sinusoids == %d should be 1\n", n_sinusoids);
	}
	sinusoid_t d = subtract_sinusoids1(estimated[0], s);
	dprintf(11, "frequency=%g estimated_frequency=%g relative error=%g\n", s.frequency, estimated[0].frequency, d.frequency);
	dp(11, "amplitude=%g estimated_amplitude=%g relative error=%g\n", s.amplitude, estimated[0].amplitude, d.amplitude);
	dp(10, "phase=%g estimated_phase=%g error=%g\n", s.phase,estimated[0].phase, d.phase);

	assert(fabs(d.frequency) < MAXIMUM_FREQUENCY_ERROR);
	assert(fabs(d.amplitude) < MAXIMUM_AMPLITUDE_ERROR);
	assert(n_tests < MAX_TESTS);
	errors[n_tests] = d;
	n_tests++;
}

static void many_tests(int n) {
	// genenerate frequencies at least this many hz away from 0 and nyquist
	double margin = 4*MAX(param_get_double("spectral_analysis", "relative_relief_outside_radius"), param_get_double("spectral_analysis", "peak_radius"))/param_get_double("spectral_analysis","sampling_rate");
	for (int fft_length = 64; fft_length <= 1024; fft_length *= 2) {
		for (int i = 0; i < n; i++) {
			double frequency =  margin + (0.5-2*margin)*(rand() / (RAND_MAX + 1.0));
			double amplitude = 0.0001+0.5*(rand() / (RAND_MAX + 1.0));
			double phase = 2*M_PI *(rand() / (RAND_MAX + 1.0));
			single_test(fft_length, (sinusoid_t){amplitude, phase, frequency});
		}
	}
	sinusoid_t total = {};
	sinusoid_t total_squared = {};
	for (int i = 0; i < n_tests; i++) {
		total = add_sinusoids(total, errors[i]);
		total_squared = add_sinusoids(total_squared, multiply_sinusoids(errors[i], errors[i]));
	}

	dp(1, "mean frequency error=%.10f squared error = %g\n", total.frequency/n_tests, total_squared.frequency/n_tests);
	dp(1, "mean amplitude error=%.10f squared error = %g\n", total.amplitude/n_tests, total_squared.amplitude/n_tests);
	dp(1, "mean phase error=%.10f squared error = %g\n", total.phase/n_tests, total_squared.phase/n_tests);
}

static void test_find_sinusoids(void) {
	param_set_integer("spectral_analysis", "peak_radius", 8);
	single_test(512, (sinusoid_t){0.480381, 4.43585, 0.311815}); 
	param_set_integer("spectral_analysis", "peak_radius", 65);
	single_test(256, (sinusoid_t){0.197291,4.92036,0.36694});
	for (double phase = 0; phase < 3; phase += 0.2)
		single_test(256, (sinusoid_t){0.5, phase, 0.3});
	single_test(256, (sinusoid_t){0.4567, 0.789, 0.12345});
	single_test(256, (sinusoid_t){0.240941, 1.59615, 593.7490/16000}); //created  problems because it was close to pi
	many_tests(1000);
}

int
main(int argc, char*argv[]) {
	testing_initialize(&argc, &argv, "");
	g_test_add_func("/spectral_analysis/estimate_sinusoid_parameters_test find_sinusoids", test_find_sinusoids);
	return g_test_run(); 
	return 0;
}
