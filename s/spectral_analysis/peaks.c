#include "i.h"

int
bins_to_peaks(int length, power_t data[length], int peaks[length]) {
	power_t min_height = double_to_power_t(param_get_double("spectral_analysis", "peak_min_height")); // FIXME convert db to power_t
    double hertz_per_bin = param_get_double("spectral_analysis","sampling_rate")/param_get_double("spectral_analysis","fft_points");
	dp(27, "hertz_per_bin=%g (%g/%g)\n", hertz_per_bin, param_get_double("spectral_analysis","sampling_rate"), param_get_double("spectral_analysis","fft_points"));

	int inside_radius = MAX(1,param_get_double("spectral_analysis", "relative_relief_inside_radius")/hertz_per_bin);
	int outside_radius = MAX(1, param_get_double("spectral_analysis", "relative_relief_outside_radius")/hertz_per_bin);
	outside_radius = MIN(outside_radius, (length-1)/2);
	inside_radius = MIN(inside_radius, outside_radius);
	int radius = MAX(outside_radius, param_get_double("spectral_analysis", "peak_radius")/hertz_per_bin);
	radius = MIN(radius, (length-1)/2);
	double min_relative_relief = param_get_double("spectral_analysis", "min_relative_relief");
	assert(length >= radius*2 + 1);
	int n_peaks = find_peaks_radius(length, data, peaks, radius, min_height);
	dp(27, "n_peaks before filtering =%d\n", n_peaks);
	if (min_relative_relief > 1)
		n_peaks =  filter_peaks_on_relief(n_peaks, peaks, data, inside_radius, outside_radius, min_relative_relief);
	dp(27, "n_peaks after filtering =%d\n", n_peaks);
	return n_peaks;
}

int
filter_peaks_on_relief(int n_peaks, int peaks[n_peaks], power_t data[], int inside_radius, int outside_radius, double min_relative_relief) {
    int n_filtered_peaks = 0;
	dp(27, "min_relative_relief=%g inside_radius=%d outside_radius=%d\n", min_relative_relief, inside_radius, outside_radius );
	if (!min_relative_relief)
		return n_peaks;
	for (int i = 0; i < n_peaks; i++) {
		int reject = 0;
		int p = peaks[i];
		for (int k = -outside_radius; k <= outside_radius; k++) {
			if (k >= -inside_radius && k <= inside_radius)
				continue;
			// if (log(power_t_to_double(data[i])) - log(power_t_to_double(data[i+k])) < min_relative_relief) {
			if (power_t_to_double(data[p])/power_t_to_double(data[p+k]) < min_relative_relief) {
				reject = 1;
//				dp(0, "rejecting peak[%d] data[%d]=%g data[%d]=%g\n", i, p, power_t_to_double(data[p]), p+k, power_t_to_double(data[p+k]));
				break;
			}
		}
		if (!reject)
			peaks[n_filtered_peaks++] = p;
	}
	return n_filtered_peaks;
}

// speed of this function is important
/**
 * Return a list of local maxima 
 * @param[length] number of elements in array
 * @param[data] array
 * @param[peaks] set to indices of local maxima found
 * @returns the number of local maxima found
 *
 * @note radius & minimum height requirements for local maxmima
 * take from parameters
 *
 * The speed of this function is important (simple
 * implementation took more time than FFT) so
 * heap-based implementation used.
 */

int
find_peaks_radius(int length, power_t * restrict data, int * restrict peaks, int radius, power_t min_height) {
	dp(27, "find_peaks_radius(length=%d,radius=%d, min_height=%g)\n", length, radius,  power_t_to_double(min_height));
	if (verbosity > 29) {
		for (int i = 0; i < length; i++)
			dp(30, "data[%d]=%g\n", i, power_t_to_double(data[i]));
	}
	int heap[length/2+1];
	for (int i = 0; i < length/2; i++)
		heap[i] = data[2*i] > data[2*i+1] ? 2*i : 2*i+1;
	int heap_limit = length/2;
	if (length % 2 == 1)
		heap[heap_limit++] = length - 1;
	int heap_depth = 2;
//	for (int i = 0; i < heap_limit; i++)
//		dp(20, "%d,", m[i]);
//	dp(20, "\n");
	while (2*heap_depth <= radius + 1) {
//		dp(20, "r=%d heap_limit=%d\n", r, heap_limit);
		for (int i = 0; i < heap_limit/2; i++)
			heap[i] = data[heap[2*i]] > data[heap[2*i+1]] ? heap[2*i] : heap[2*i+1];
		if (heap_limit % 2) {
			heap[heap_limit/2] = heap[heap_limit-1];
			heap_limit = heap_limit/2 + 1;
		} else {
			heap_limit = heap_limit/2 ;
		}
//		for (int i = 0; i < heap_limit; i++)
//			dp(20, "%d,", m[i]);
//		dp(20, "\n");
		heap_depth *= 2;
	}
	int n_peaks = 0;
	for (int p = 0; p < heap_limit; p++) {
		int i = heap[p];
		if (i < radius || i >= length-radius)
			continue;
		if (data[i] <= min_height)
			continue;
		int r = (i/heap_depth)*heap_depth;
		for (int j = i - radius; j < r; j++)
			if (data[j] > data[i])
				goto done; // goto faster
		for (int j = r + heap_depth ; j <= i + radius; j++)
			if (data[j] > data[i])
				goto done;
		peaks[n_peaks++] = i;
		dp(27, "peak at %d\n", i);
		done:;
	}
	return n_peaks;
}

