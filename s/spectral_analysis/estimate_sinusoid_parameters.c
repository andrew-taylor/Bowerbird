#include "i.h"
static double 
r(double n, double omega) {
	if (approximately_zero(omega))
		return  n + 1;
	else
		return sin(omega*(n + 1)/2)/sin(omega/2);
}

static double
w(double n, double omega) {
	return 0.5*r(n,omega) + 0.25*r(n,omega-2*M_PI/n) + 0.25*r(n,omega+2*M_PI/n);
}

sinusoid_t
calculate_refined_sinusoid_parameters(track_t *t, track_point_t *tp) {
	sinusoid_t s;
	int n_bins = t->fft.n_bins;
	dp(29, "power[%d]=%g\n", tp->bin, power_t_to_double(tp->power[1]));
  	double frequency_delta = parabolic_frequency_interpolation(tp->power, 1);
	s.frequency = (tp->bin + frequency_delta)/(2.0*n_bins);
	dp(28, "frequency_delta=%g frequency %g -> %g\n", frequency_delta, tp->bin/(2.0*n_bins), s.frequency);
	double corrected_power = parabolic_amplitude_interpolation(tp->power, 1);
 	double amplitude_correction = w(2*n_bins,M_PI*frequency_delta/n_bins)/n_bins;
	s.amplitude = sqrt(1.49*corrected_power/n_bins); // FIXME -   1.49 is an experimentally determined kludge
	dp(28, "amplitude correction %g s.amplitude=%g\n", amplitude_correction, s.amplitude);
	s.phase = tp->phase[1] ? estimate_phase(1, frequency_delta, tp->phase) : 0;
	return s;
}

sinusoid_t
estimate_sinusoid_parameters(int bin, int n_bins, power_t power[n_bins], phase_t phase[n_bins], int approximate) {
	sinusoid_t s;
	if (approximate) {
		s.frequency = bin/(2.0*n_bins);
		s.amplitude = 1;
		s.phase = 0;
		return s;
	}
	dp(29, "power[%d]=%g\n", bin, power_t_to_double(power[bin]));
  	double frequency_delta = parabolic_frequency_interpolation(power, bin);
	s.frequency = (bin + frequency_delta)/(2.0*n_bins);
	dp(28, "frequency_delta=%g frequency %g -> %g\n", frequency_delta, bin/(2.0*n_bins), s.frequency);
	double corrected_power = parabolic_amplitude_interpolation(power, bin);
 	double amplitude_correction = w(2*n_bins,M_PI*frequency_delta/n_bins)/n_bins;
	s.amplitude = sqrt(1.49*corrected_power/n_bins); // FIXME -   1.49 is an experimentally determined kludge
	dp(28, "amplitude correction %g s.amplitude=%g\n", amplitude_correction, s.amplitude);
	s.phase = phase ? estimate_phase(bin, frequency_delta, phase) : 0;
	return s;
}

// quick hack that needs to be rewritten 
// if estimating phase is important
double
estimate_phase(int bin, double frequency_delta, phase_t phase[]) {
	dp(30, "bin=%d frequency_delta=%g\n", bin, frequency_delta);
	if (frequency_delta < 0) {
		bin--;
		frequency_delta++;
	}
	dp(30, "bin=%d frequency_delta=%g\n", bin, frequency_delta);
	double phase1 = phase_t_to_double(phase[bin]);
	double phase2 = phase_t_to_double(phase[bin+1]);
	double delta = phase1 - phase2;
	if (fabs(delta) > M_PI) {
		if (phase1 < phase2)
			phase1 += 2*M_PI;
		else
			phase2 += 2*M_PI;
	}
	dp(30, "phase1=%g phase2=%g\n", phase1, phase2);
	double weighted_average = (1 - frequency_delta) * phase1 + frequency_delta*phase2;
	dp(30, "delta=%g weighted_average=%g\n", delta, weighted_average);
	if (weighted_average >= 2*M_PI)
		weighted_average -=  2*M_PI;
	return weighted_average;
}

// See for example, http://www.cs.tut.fi/sgn/arg/music/tuomasv/MScThesis.pdf
double
parabolic_frequency_interpolation(power_t *power, int bin) {
	double p1 = power_t_to_double(power[bin-1]);
	double p2 = power_t_to_double(power[bin]);
	double p3 = power_t_to_double(power[bin+1]);
	if (!p1||!p2||!p3)
		return p2;
	double m1 = log(p1);
	double m2 = log(p2);
	double m3 = log(p3);
//	dp(31, "%g %g %g %g %g %g\n", p1, p2, p3, m1,m2,m3);
	double denominator = m1 - 2*m2 + m3;
	if (approximately_zero(denominator))
		return p2;
	return 0.5*(m1 - m3)/denominator;
}	

double
parabolic_amplitude_interpolation(power_t *power, int bin) {
	double p1 = power_t_to_double(power[bin-1]);
	double p2 = power_t_to_double(power[bin]);
	double p3 = power_t_to_double(power[bin+1]);
	if (!p1||!p2||!p3)
		return p2;
	double m1 = log(p1);
	double m2 = log(p2);
	double m3 = log(p3);
	double denominator = m1 - 2*m2 + m3;
//	dp(0, "%g %g %g %g %g %g %g %g\n",p1,p2,p3,m1,m2,m3,denominator,m2 - 0.125*(m1 - m3)*(m1 - m3)/denominator);
	if (approximately_zero(denominator))
		return p2;
	return exp(m2 - 0.125*(m1 - m3)*(m1 - m3)/denominator);
}	
/*
 * see http://www.unibw-hamburg.de/EWEB/ANT/dafx2002/papers/DAFX02_Keiler_Marchand_sine_extract_compare.pdf
 */
#ifdef UNUSED

void
reassignment_frequency_correction(sample_t *samples, index_t n_samples, index_t frame_length, index_t fft_length, double sampling_rate, double correct_frequency) {
	double binwidth = sampling_rate*(1.0/(fft_length/2.0))*(1/2.0);
  	frame_length = min_i(frame_length, min_i(fft_length, n_samples));
	int n_bins = (fft_length+1)/2;
	double window[fft_length];
	double frequency_window[fft_length];
    double window_correction = 0,frequency_window_correction = 0;
	for (int k = 0; k < fft_length; k++) {
		window[k] = 0.5*(1-cos(2*M_PI*(k+0.5)/fft_length));
		window_correction += window[k]*window[k];
		dp(31, "window[%d]=%g\n", k, window[k]);
	}
	for (int k = 1; k < fft_length-1; k++)
		frequency_window[k] = 0.5*(window[k+1]-window[k-1]);
	frequency_window[0] = 0.5*window[1];
	frequency_window[fft_length-1] = -0.5*window[fft_length-2];
	for (int k = 0; k < fft_length; k++)
		frequency_window_correction += frequency_window[k]*frequency_window[k];
	double frequency_window1[fft_length];
	for (int k = 0; k < fft_length; k++)
		frequency_window1[k] = sin(2*M_PI*(k+0.5)/fft_length);
//	gnuplotf("width=1200 height=1000 rows=2 title='hann' length=%d %lf;title='frequency' %lf title='frequency1' %lf", fft_length, window, frequency_window,frequency_window1);exit(0);
	double in[fft_length+2];
	for (int k = 0; k < fft_length; k++)
		in[k] = window[k]*samples[k];
	fftw_complex out[fft_length+2];
	fftw_execute(fftw_plan_dft_r2c_1d(fft_length, in, out, FFTW_ESTIMATE));
	for (int k = 0; k < fft_length; k++)
		in[k] = frequency_window[k]*samples[k];
	fftw_complex frequency_out[fft_length+2];
	fftw_execute(fftw_plan_dft_r2c_1d(fft_length, in, frequency_out, FFTW_ESTIMATE));
//	gnuplotf("width=1200 height=1000 rows=2 title='hann' length=%d %lf;title='frequency' %lf", fft_length, out, frequency_out);
	
	double power[n_bins] ;
	for (int k = 0; k < n_bins; k++) { 
		double real = out[k][0];
		double imaginary = out[k][1];
		power[k] = (real*real + imaginary*imaginary);
	}
	for (int bin = 1; bin < n_bins-1; bin++) {
		if (power[bin] < 0.0001)
			continue;
 		if ((bin > 0 && power[bin-1] >= power[bin]) || (bin < n_bins - 1 && power[bin+1] >= power[bin]))
  			continue;
		//calculate complex conjugate for complex division
	    double deviation = -(frequency_out[bin][1] *out[bin][0] - frequency_out[bin][0] * out[bin][1]);
	    deviation *= 2*sqrt(frequency_window_correction)/sqrt(window_correction);
		printf("reassignment bin=%d amplitude=%g frequency=%g deviation=%g\n", bin, sqrt(power[bin]), ((double)bin + deviation) * binwidth, deviation);
		printf("error=%g\n", deviation*binwidth/(correct_frequency-bin*binwidth));
	}
}
/*
 * returns sinusoids in decreasing order of amplitude
 */
int
estimate_sinusoid_parameters(int n_bins, double power[n_bins], double phase[n_bins], double next_phase[n_bins], int overlap, double threshold, int frequency_correction_method, int max_sinusoids, sinusoid_t sinusoids[max_sinusoids]) {
	double local_maxima[n_bins], peak_energy[n_bins],  peak_frequency[n_bins], **sorted_energies;
	int n_sinsuoids, i, n_peaks;
	double power_dominant, amplitude_dominant;
	double hop = 1.0/overlap;
		
	dp(13, "estimate_sinusoid_parameters(n_bins =%d max_sinusoids=%d threshold=%g)\n", n_bins, max_sinusoids, threshold);
	memset(local_maxima, 0, sizeof local_maxima);
	
	power_dominant = 0;
	amplitude_dominant = 0;
	for (i = 1; i < n_bins - 1; i++) {
		if (power[i] > 0 && power[i] >= power[i - 1] && power[i] >= power[i + 1]) {
			local_maxima[i] = 1;
			if (power[i] >= power_dominant)
				power_dominant = power[i];
		}
	}
//	for (i = 0; i < n_bins; i++)
//		dp(31, "power[%d]=%g\n", i, power[i]); 
	dp(25, "uncorrected power_dominant=%g (uncorrected)\n", power_dominant); 
	
	n_peaks = 0;
	for (i = 2; i < n_bins - 1; i++) {
		int start = i;
		if (!local_maxima[i])
			continue;
		for (; local_maxima[i]; i++)
			local_maxima[i] = 0;
		int bin = (start + i)/2;
		dp(29, "power[%d]=%g\n", bin,power[bin]);
		if (power[bin] < power_dominant * (threshold*threshold)*0.5) 
			continue;
		
 		int use_parabolic_interpolation = frequency_correction_method == 1;
      	double frequency_delta = 0;
		if (frequency_correction_method == 0) {
	    	double raw_phase_diff = next_phase[bin] - phase[bin];
	     	double phase_diff = raw_phase_diff - bin * 2 * M_PI * hop;
		    dp(31, "phase_diff=%g\n", phase_diff);
	     	double phase_diff_div_pi = phase_diff/M_PI;
		    double x = fabs(phase_diff-((int)phase_diff_div_pi)*M_PI);
		    dp(31, "x=%g\n", x);
		    // phase difference corrections seems unstable where x ~= 1
		    if (x > 0.99 || x < 0.01)
		    	use_parabolic_interpolation = 1;
		    else {
         		int piSigner = phase_diff_div_pi;
	     		if (piSigner >= 0)
	     	 		piSigner += piSigner & 1;
	     		else
	     			piSigner -= piSigner & 1;
	        	phase_diff -= M_PI * (double)piSigner;
	 	       frequency_delta = (overlap * phase_diff)/(2*M_PI);
	 	       dp(31, "raw_phase_diff=%g expect_phased_diff=%g  raw_phase_diff/pi=%g corrected phase diff=%g frequency_delta=%g\n", raw_phase_diff, bin * 2 * M_PI * hop, raw_phase_diff/M_PI, phase_diff, frequency_delta);
	     	}
	 	}
	 	if (use_parabolic_interpolation)
			frequency_delta =  parabolic_frequency_interpolation(power, bin);
		peak_frequency[n_peaks] = bin + frequency_delta;
		
		/*
		 * amplitude correction from http://www.unibw-hamburg.de/EWEB/ANT/dafx2002/papers/DAFX02_Keiler_Marchand_sine_extract_compare.pdf
		 */
		double uncorrected_amplitude = 2*sqrt(power[bin]/(2*n_bins)); 
	 	double amplitude_correction = w(2*n_bins,M_PI*frequency_delta/n_bins)/n_bins;
		peak_energy[n_peaks] = amplitude_correction*2*sqrt(power[bin]/(2*n_bins));
		double corrected_power = parabolic_amplitude_interpolation(power, bin);
		dp(11, "parabolic=%g\n", 2*sqrt(corrected_power/(2*n_bins)));
		peak_energy[n_peaks] = 2*sqrt(corrected_power/(2*n_bins));
		dp(11, "bin=%d frequency_delta=%g uncorrected amplitude=%g amplitude_correction=%g corrected amplitude=%g\n", bin, frequency_delta, uncorrected_amplitude, amplitude_correction, peak_energy[n_peaks]); 
		if (peak_energy[n_peaks] > amplitude_dominant) 
			amplitude_dominant = peak_energy[n_peaks]; /* power_dominant is uncorrected */

		n_peaks++;
	}
	

	sorted_energies = quicksort_double_indices(peak_energy, n_peaks);
	
	if (verbosity > 21) {
		for (i = 0; i < min_i(10, n_peaks); i++)
			fprintf(debug_stream, "i=%d j=%d energy=%g\n", i, sorted_energies[n_peaks - (i+1)] - &peak_energy[0], peak_energy[sorted_energies[n_peaks - (i+1)] - &peak_energy[0]]); 
	}
	
	n_sinsuoids = min_i(max_sinusoids, n_peaks);
	for (i = 0; i < n_sinsuoids; i++) {
		int j = sorted_energies[n_peaks - (i+1)] - &peak_energy[0];
		dp(23, "i=%d j=%d frequency=%g peak_energy[%d]=%g amplitude_dominant=%g\n", i, j, peak_frequency[j], j, peak_energy[j], amplitude_dominant); 
		if (peak_energy[j] < amplitude_dominant*threshold) { 
			dp(23, "ignoring this and further peaks\n"); 
			break;
		}
		if (sinusoids) {
			sinusoids[i].frequency = peak_frequency[j];
			sinusoids[i].amplitude = peak_energy[j];
			sinusoids[i].phase = phase[(int)peak_frequency[j]];
		}
	}
	g_free(sorted_energies);
	return i;
}
#endif
