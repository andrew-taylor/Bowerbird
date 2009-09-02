#include "i.h"

double
create_hann_window(double *window, uint32_t n_window) {
	double squared_window_coefficients = 0;
	for (int k = 0; k < n_window; k++) {
		window[k] = 0.5*(1-cos(2*M_PI*(k+0.5)/n_window));
		squared_window_coefficients += window[k]*window[k];
		dp(35, "window[%d]=%g\n", k, window[k]);
	}
	return squared_window_coefficients;
}

double
create_hann_window_fp(uint32_t *window, uint32_t n_window) {
	double squared_window_coefficients = 0;
	for (int k = 0; k < n_window; k++) {
		double d = 0.5*(1-cos(2*M_PI*(k+0.5)/n_window));
		window[k] = lrint((((uint32_t)1)<<31)*d);
		squared_window_coefficients += d*d;
	}
	return squared_window_coefficients;
}

double
create_square_window(double *window, uint32_t n_window) {
	for (int k = 0; k < n_window; k++)
		window[k] = 1;
	return n_window;
}


double
create_square_window_fp(uint32_t *window, uint32_t n_window) {
	for (int k = 0; k < n_window; k++)
		window[k] = ((uint32_t)1)<<31;
	return n_window;
}

void
extract_power_phase(sample_t samples[], index_t length, index_t fft_length, int do_windowing, power_t *power, phase_t *phase) {
	fft_t f = {0};
	f.n_steps = 1;
	f.window_size = length;
	f.step_size  = f.window_size; // unneeded
	f.fft_size = fft_length;
	f.n_bins = (f.fft_size+1)/2;
	if (!do_windowing) {
		if (param_get_integer("spectral_analysis", "use_fftw")) {
			f.window = salloc(f.window_size*sizeof (double));
			create_square_window(f.window, f.window_size);
		} else {
			f.window = salloc(f.window_size*sizeof (uint32_t));
			create_square_window_fp(f.window, f.window_size);
		}
		f.window_correction = 1.0/f.n_bins;
	}
	short_time_power_phase(samples, &f, (void *)power, (void *)phase);
	free_fft(&f);
}

void
short_time_power_phase(sample_t samples[], fft_t *f, power_t power[f->n_steps][f->n_bins], phase_t phase[f->n_steps][f->n_bins]) {
	int use_fftw = param_get_integer("spectral_analysis", "use_fftw");
	assert(f->n_steps > 0);
	assert(f->window_size > 0 && f->window_size <= f->fft_size);
	assert(f->step_size > 0 && f->step_size <= f->window_size);
	assert(samples && power && f->fft_size > 0);
	if (!f->window) {
		f->window = salloc(f->window_size*sizeof (double));
		double squared_window_coefficients = use_fftw ? create_hann_window(f->window, f->window_size) : create_hann_window_fp(f->window, f->window_size);
		f->window_correction = f->window_size/(squared_window_coefficients*f->n_bins);
	}
	if (use_fftw)
		fftw_short_time_power_phase(samples, f, power, phase);
	else
		kiss_short_time_power_phase(samples, f, power, phase);
}

void
free_fft(fft_t *f) {
	if (f->window) g_free(f->window);
	if (param_get_integer("spectral_analysis", "use_fftw")) {
#ifdef USE_FFTW
		if (f->in) {
			fftw_destroy_plan(f->state);
			fftw_free(f->in);
		}
		if (f->out) fftw_free(f->out);
#endif
	} else {
#ifdef USE_KISS_FFT
		if (f->in) g_free(f->in);
		if (f->out) g_free(f->out);
#endif
	}
}

void
fftw_short_time_power_phase(sample_t samples[], fft_t *f, power_t power[f->n_steps][f->n_bins], phase_t phase[f->n_steps][f->n_bins]) {
#ifndef USE_FFTW
	die("fftw not compiled in");
#else
	if (!f->window) {
		f->window = salloc(f->window_size*sizeof (double));
		double squared_window_coefficients = create_hann_window(f->window, f->window_size);
		f->window_correction = f->window_size/(squared_window_coefficients*f->n_bins);
	}
	if (!f->in) {
		f->in = fftw_malloc(f->fft_size*sizeof (double));
		dp(25, "fftw_malloc(%d)\n", (int)((f->window_size+2)*sizeof (fftw_complex)));
		f->out = fftw_malloc((f->n_bins+2)*sizeof (fftw_complex));  // valgrind complains without the +2`
		dp(28, "fftw_plan_dft_r2c_1d(%d, %p, %p, FFTW_ESTIMATE)\n", f->window_size, f->in, f->out);
		f->state = fftw_plan_dft_r2c_1d(f->fft_size, f->in, f->out, FFTW_ESTIMATE);
	}
	double * restrict in = f->in;
	fftw_complex * restrict out = f->out;
	fftw_plan plan = f->state;
	for (int step = 0; step < f->n_steps; step++) {
		for (int k = 0; k < f->window_size; k++)
			in[k] = ((double *)f->window)[k]*sample_t_to_double(samples[k+step*f->step_size]);
		for (int k = f->window_size; k < f->fft_size; k++)
			in[k] = 0;
		dp(30, "fftw_execute(%p)\n", f->state);
		fftw_execute(plan);
		dp(31, "fftw_execute returns\n");
		for (int k = 0; k < f->n_bins; k++) {
			double real = out[k][0];
			double imaginary = out[k][1];
			power[step][k] = double_to_power_t(f->window_correction*(real*real + imaginary*imaginary));
//			dp(1, "%d real=%g imaginary=%g %g\n", k, real, imaginary, power[step][k]);
			if (phase)
				phase[step][k] = double_to_phase_t(M_PI/2+atan2(imaginary,real));
		}	
		power[step][0] /= 2;
		if (verbosity >= 31) {
			for (int k = 0; k < f->n_bins; k++)
				fprintf(debug_stream, "%g ", power_t_to_double(power[step][k]));
			fprintf(debug_stream, "\n");
		}
	}
#endif
}

void
kiss_short_time_power_phase(sample_t samples[], fft_t *f, power_t power[f->n_steps][f->n_bins], phase_t phase[f->n_steps][f->n_bins]) {
#ifndef USE_KISS_FFT
	die("KISS FFT not compiled in");
#else
	if (!f->in) {
		f->in = salloc((f->fft_size+2)*sizeof (kiss_fft_scalar));
		f->out = salloc(f->fft_size*sizeof (kiss_fft_cpx));
		f->state = kiss_fftr_alloc(f->fft_size, 0, 0, 0);
	}
    kiss_fft_scalar *rin = f->in;
    kiss_fft_cpx *rout = f->out;
	kiss_fftr_cfg kiss_fftr_state = f->state;
#ifdef DOUBLE_POWER_T
	double correction = f->window_correction*f->fft_size*f->fft_size;
#else
	uint64_t correction = f->window_correction*f->fft_size*f->fft_size + 0.5;
//	dp(1, "%g %g %d %g\n", (double)correction, f->window_correction, f->fft_size, f->window_correction*f->fft_size*f->fft_size);
#endif
	assert(31+SAMPLE_T_BIT_SHIFT-KISS_BIT_SHIFT >= 0);
	for (int step = 0; step < f->n_steps; step++) {
		for (int k = 0; k < f->window_size; k++)
			rin[k] = (((uint32_t *)f->window)[k] * ((int64_t)samples[k+step*f->step_size])) / ((uint64_t)1 << (31+SAMPLE_T_BIT_SHIFT-KISS_BIT_SHIFT));
		for (int k = f->window_size; k < f->fft_size; k++)
			rin[k] = 0;
		kiss_fftr(kiss_fftr_state, rin, rout);
		for (int k = 0; k < f->n_bins; k++) {
#ifdef DOUBLE_POWER_T
			double real = kiss_to_double(rout[k].r);
			double imaginary = kiss_to_double(rout[k].i);
			double p = double_to_power_t(correction*(real*real + imaginary*imaginary));
#else
			uint64_t p;
#if POWER_T_BIT_SHIFT < KISS_BIT_SHIFT
			p = (correction*((((uint64_t)(((int64_t)rout[k].r)*rout[k].r + ((int64_t)rout[k].i)*rout[k].i))) >> KISS_BIT_SHIFT)) >> (KISS_BIT_SHIFT - POWER_T_BIT_SHIFT);
#else
			p = (correction*((((uint64_t)(((int64_t)rout[k].r)*rout[k].r + ((int64_t)rout[k].i)*rout[k].i))) >> KISS_BIT_SHIFT)) << (POWER_T_BIT_SHIFT - KISS_BIT_SHIFT);
#endif
#endif
#ifdef POWER_T_MAX
			power[step][k] = p > POWER_T_MAX ? POWER_T_MAX : p;
#else
			power[step][k] = p;
#endif
			if (phase)
				phase[step][k] = double_to_phase_t(M_PI/2+atan2(kiss_to_double(rout[k].i),kiss_to_double(rout[k].r)));
		}	
		power[step][0] /= 2;
		if (verbosity >= 29) {
			for (int k = 0; k < f->n_bins; k++)
				fprintf(debug_stream, "%g ", power_t_to_double(power[step][k]));
			fprintf(debug_stream, "\n");
		}
	}
#endif
}

