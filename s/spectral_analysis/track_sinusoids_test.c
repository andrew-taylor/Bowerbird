#include "i.h"

#ifdef OLD
void
compare_sinusoids(sinusoid_t s1, sinusoid_t s2) {
	sinusoid_t s = subtract_sinusoids(s1, s2);
	double frequency_error = 0;
	if (s1.frequency)
		s.frequency = s.frequency/s1.frequency;
	double amplitude_error = 0;
	if (s1.amplitude)
		amplitude_error = s.amplitude/s1.amplitude;
	double phase_error = s.phase;
	if (phase_error > M_PI)
		phase_error -= 2* M_PI;
	else if (phase_error < -M_PI)
		phase_error += 2* M_PI;
	dp(27, "frequency=%g estimated_frequency=%g frequency_error=%g\n",s1.frequency, s2.frequency, frequency_error);
	dp(27, "amplitude=%g estimated_amplitude=%g amplitude_error=%g\n",s1.amplitude, s2.amplitude, amplitude_error);
	dp(27, "phase=%g estimated_phase=%g phase_error=%g\n",s1.phase, s2.phase, phase_error);
	assert(fabs(frequency_error) < 0.01);
	assert(fabs(amplitude_error) < 0.1);
//	assert(fabs(phase_error) < 0.1);
}

static void test_track_sinusoids1(sinusoid_t s) {
	param_set_double("spectral_analysis", "peak_min_height", 0.000001*s.amplitude);
	fft_t f = {0};
	f.n_steps = 20;
	f.window_size = 256;
	f.step_size  = f.window_size/2;
	f.fft_size = f.window_size;
	f.n_bins = (f.fft_size+1)/2;
	int n_samples = f.window_size + (f.n_steps-1) * f.step_size;
	sample_t samples[n_samples];
	set_sinusoid(samples, n_samples, s);
	if (verbosity > 30) {
		for (int i = 0; i < n_samples; i++)
			dp(31, "samples[%d] = %g %g %g\n", i, (double)samples[i], sample_t_to_double(samples[i]), (double)double_to_sample_t(sample_t_to_double(samples[i])));
	}
	power_t power[f.n_steps][f.n_bins];
	phase_t phase[f.n_steps][f.n_bins];
	short_time_power_phase(samples, &f, power, phase);
	GArray *tracks = track_sinusoids(f, power, phase);
	assert(tracks->len == 1);
	track_t *t = &g_array_index(tracks, track_t, 0);
	assert(t->start == 0);
	assert(t->completed == 0);
	assert(t->points->len == f.n_steps);
	for (int i = 0; i < f.n_steps; i++)
		compare_sinusoids(s, g_array_index(t->points, sinusoid_t, i));
}

static void test_track_sinusoids(void) {
	test_track_sinusoids1((sinusoid_t){0.4567, 0.789, 0.12345});
}
#endif

int
main(int argc, char*argv[]) {
	testing_initialize(&argc, &argv, "");
#ifdef OLD
	g_test_add_func("/spectral_analysis/track_sinusoids track_sinusoids", test_track_sinusoids);
#endif
	return g_test_run(); 
	return 0;
}
