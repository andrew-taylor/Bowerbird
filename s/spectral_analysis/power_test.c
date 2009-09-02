#include "i.h"

#ifdef SIMPLE_FFT_TEST
#include <fftw3.h>
void
simple_fft(void) {
	int fft_size = 256;
	double *in = fftw_malloc(fft_size* sizeof *in);
	fftw_complex *out = fftw_malloc(fft_size* sizeof *out);
	fftw_plan p;
	int i,j;

	p = fftw_plan_dft_r2c_1d(fft_size, in, out, FFTW_ESTIMATE);

	double amplitude[] = {0.1,0.1,0.1};
	double phase[] = {0.3,1.3,2.3};
	double frequency[] = {0.125,0.250,0.0625};
	for (i = 0; i < sizeof amplitude/sizeof *amplitude; i++)
			printf("Actual:    frequency=%g amplitude=%g phase=%g\n",frequency[i], amplitude[i], phase[i]);
	printf("\n");
	
	for (i = 0; i < fft_size; i++) {
		in[i] = 0;
		for (j = 0; j < sizeof amplitude/sizeof *amplitude; j++)
			in[i] +=  amplitude[j]*sin(phase[j] + i*2*M_PI*frequency[j]);
	}
	fftw_execute(p);
	 
	for (i = 1; i < (fft_size+1)/2; i++) {
		double frequency = i/(double)fft_size;
		double real = out[i][0];
		double imaginary = out[i][1];
		double amplitude = 2*sqrt(real*real + imaginary*imaginary)/fft_size;
		double phase = -atan2(imaginary,real);
		if (imaginary > 0)
			phase += M_PI;
		/* ignore tiny but sinusoids produced by rounding */
		if (amplitude > 0.001)
			printf("Estimated: frequency=%g amplitude=%g phase=%g\n",frequency, amplitude, phase);
	}
}
#endif

#ifdef FIXED_POINT
#define MAXIMUM_ERROR 0.01
#else
#define MAXIMUM_ERROR 0.01
#endif

void test_power_phase(void) {
	int length = 512;
	int spectrum_bins = (length+1)/2;
	sample_t samples[length];
	set_sinusoid1(samples, length, 0.10, 0.0, 0.321);
	if (verbosity > 30) {
		for (int i = 0; i < length; i++)
			dp(31, "samples[%d] = %g %g %g\n", i, (double)samples[i], sample_t_to_double(samples[i]), (double)double_to_sample_t(sample_t_to_double(samples[i])));
	}
	double power = 0,power1=0,power2=0,power3=0;
	for (int i = 0; i < length; i++)
		power += sample_t_to_double(samples[i]) * sample_t_to_double(samples[i]);
	int multiply = 8;
	power_t spectrum[multiply*spectrum_bins];
	extract_power_phase(samples, length, length, 0, spectrum, NULL);
	for (int i = 0; i < spectrum_bins; i++) {
		assert(spectrum[i] >= 0);
		assert(power_t_to_double(spectrum[i]) >= 0);
		power1 += power_t_to_double(spectrum[i]);
	}
	extract_power_phase(samples, length, length, 1, spectrum, NULL);
	for (int i = 0; i < spectrum_bins; i++)
		power2 += power_t_to_double(spectrum[i]);
	extract_power_phase(samples, length, length*multiply, 1, spectrum, NULL);
	for (int i = 0; i < spectrum_bins*multiply; i++)
		power3 += power_t_to_double(spectrum[i]);
	dp(1, "power %g %g %g %g\n", power, power1, power2,power3);
	assert(fabs((power1-power)/power) < MAXIMUM_ERROR);
	assert(fabs((power2-power)/power) < MAXIMUM_ERROR);
	assert(fabs((power3-power)/power) < MAXIMUM_ERROR);
	
}

static void test_short_time_power_phase(void) {
	fft_t f = {0};
	f.n_steps = 20;
	f.window_size = 64;
	f.step_size  = f.window_size/2;
	f.fft_size = 8*f.window_size;
	f.n_bins = (f.fft_size+1)/2;
	int n_samples = f.window_size + (f.n_steps-1) * f.step_size;
	sample_t samples[n_samples];
	for (int i = 0; i < n_samples; i++)
		samples[i]  = double_to_sample_t(0.2);
	set_sinusoid1(samples, n_samples, 0.10, 0.0, 0.0123);
	add_sinusoid1(samples, n_samples, 0.12, 2.9, 0.04567);
	add_sinusoid1(samples, n_samples, 0.17, 3.5, 0.078901);
	add_sinusoid1(samples, n_samples, 0.36, 4.2, 0.3434);
	double power = 0, power1 = 0, power2 = 0;
	for (int i = 0; i < n_samples; i++)
		power += sample_t_to_double(samples[i])*sample_t_to_double(samples[i]);
	power_t spectrum[f.n_steps][f.n_bins];
	short_time_power_phase(samples, &f, spectrum, NULL);
	for (int step = 0; step < f.n_steps; step++) {
		for (int j = 0; j < f.n_bins; j++)
			power1 +=  power_t_to_double(spectrum[step][j]);
	}
	for (int step = 0; step < f.n_steps; step++) {
		power_t spectrum1[f.n_bins];
		extract_power_phase(samples+step*f.step_size, f.window_size, f.fft_size, 1, spectrum1, NULL);
		for (int j = 0; j < f.n_bins; j++)
			power2 +=  power_t_to_double(spectrum1[j]);
	}
	
	
	// correct for overlapping steps
	power1 *= n_samples/(double)(f.n_steps*f.window_size);
	power2 *= n_samples/(double)(f.n_steps*f.window_size);
	dp(1, "power calculated directly from signal        = %g\n", power);
	dp(1, "power calculated from short_time_power_phase = %g error = %g%%\n", power1, fabs(100-100*power1/power));
	dp(1, "power calculated from power_phase            = %g error = %g%%\n", power2, fabs(100-100*power2/power));
	
#ifdef DO_IMAGE
	for (int step = 0; step < f.n_steps; step++)
		for (int i = 0; i < f.n_bins; i++)
			if (spectrum[step][i])
				spectrum[step][i] = double_to_power_t(log(power_t_to_double(spectrum[step][i])));
	xv_double_array(n_steps, f.n_bins, spectrum, 0, 0, 1);
#endif
//	for (int i = 0; i < 100000;i++) power_phase(samples, window_size, window_size, 1);
	assert(fabs((power1-power)/power) < MAXIMUM_ERROR);
	assert(fabs((power2-power)/power) < MAXIMUM_ERROR);
}
 
int 
main(int argc, char*argv[]) {
	testing_initialize(&argc, &argv, "");
	g_test_add_func("/spectral_analysis/power short_time_power_phase", test_short_time_power_phase);
	g_test_add_func("/spectral_analysis/power power_phase", test_power_phase);
	return g_test_run(); 
}

