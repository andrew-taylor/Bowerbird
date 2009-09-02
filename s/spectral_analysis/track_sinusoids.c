#include "i.h"

GArray *
new_update_sinusoid_tracks(fft_t fft, power_t *power, power_t *previous_power, phase_t phase[restrict fft.n_bins], GArray *active_tracks, int step) {
	GArray *completed_tracks = g_array_new(0, 1, sizeof (track_t));
	int peaks[fft.n_bins];
	int n_peaks = bins_to_peaks(fft.n_bins, power, peaks);
	uint32_t peak_used_in_track[fft.n_bins];
	memset(peak_used_in_track, 0, sizeof peak_used_in_track);
	uint32_t is_peak[fft.n_bins];
	memset(is_peak,0, sizeof is_peak);
	for (int i = 0; i < n_peaks; i++)
		is_peak[peaks[i]] = 1;
	double seconds_per_step = fft.step_size/fft.sampling_rate;
	int max_frequency_delta = MAX(1,0.5+param_get_double("spectral_analysis", "max_frequency_delta") * seconds_per_step);
	double min_power_between_track_bins = param_get_double("spectral_analysis", "min_power_between_track_bins");
	int min_track_length = 0.5+param_get_double("spectral_analysis", "min_track_length")/seconds_per_step;
	int max_gap_size = 0.5+param_get_double("spectral_analysis", "max_gap_size")/seconds_per_step;;
	dp(22, "max_frequency_delta=%g max_gap_size=%d min_track_length=%d active_tracks=%d\n", (double)max_frequency_delta, (int)max_gap_size, (int)min_track_length, (int)active_tracks->len); 
	
	for (int i = 0; i < active_tracks->len; i++) {
		track_t *t = &g_array_index(active_tracks, track_t, i);
		if (t->completed)
			continue;
		track_point_t last_track_point = g_array_index(t->points, track_point_t, t->points->len-1); // take copy as index could change with reallocs due to appends
		dp(25, "i=%d last_track_point.bin=%d\n", i, last_track_point.bin);
		int contour_invalid[2] = {0,0};
		int next_bin = -1;
		for (int delta = 0; delta < max_frequency_delta && next_bin < 0; delta++) {
			for (int sign = 0; sign < 2; sign ++) {
				if (contour_invalid[sign])
					continue;
				int bin = last_track_point.bin + (sign*2-1) * delta;
				if (bin < 0 || bin >= fft.n_bins)
					continue;	
//				dp(1, "delta=%d is_peak[%d]=%d\n", delta, bin, is_peak[bin]);
				if (!is_peak[bin]||peak_used_in_track[bin]) {
					if (delta == 0) break; // no need to check -0 
					continue;
				}
				power_t p = MIN(power[bin], t->last_peak_power);
				
//					dp(1, "%g %g\n", power_t_to_double(last_track_point.power[1]), power_t_to_double(power[bin]));
				int d = 1; 
				for (; d < delta; d++) {
					int b = last_track_point.bin + (sign*2-1) * d;
					power_t q = MAX(power[b], previous_power[b]);
					// FIXME use FIXED POINT
//						dp(1, "d=%d %g %g %g < %g\n", d, power_t_to_double(power[b]),power_t_to_double(previous_power[b]), power_t_to_double(q)/power_t_to_double(p), min_power_between_track_bins);
					if (power_t_to_double(q)/power_t_to_double(p) < min_power_between_track_bins) {
						contour_invalid[sign] = 1;
						break;
					}
				}
				if (d >= delta) {
					next_bin = bin;
					t->last_peak_power = power[bin];
					break;
				}
			}
		}
		if (next_bin < 0 && t->gap < max_gap_size-1) {
			t->gap++;
			int max_delta = MAX(1,max_frequency_delta/2);
			int start = MAX(0, last_track_point.bin - max_delta);
			int finish = MIN(fft.n_bins-1, last_track_point.bin + max_delta);
			int max_bin = start;
			for (int b = start+1; b <= finish; b++) 
				if (power[b] > power[max_bin])
					max_bin = b;
			if (power_t_to_double(power[max_bin])/power_t_to_double(t->last_peak_power) > min_power_between_track_bins && !peak_used_in_track[max_bin])
				next_bin = max_bin;
		}
		if (next_bin >= 0) {
			dp(22, "track[%d] += (%d)\n", i, next_bin);
			peak_used_in_track[next_bin] = 1;
			track_point_t current_peak = {0};
			current_peak.bin = next_bin;
			current_peak.power[0] = power[next_bin-1];
			current_peak.power[1] = power[next_bin];
			current_peak.power[2] = power[next_bin+1];
			if (phase) {
				current_peak.phase[0] = phase[next_bin-1];
				current_peak.phase[1] = phase[next_bin];
				current_peak.phase[2] = phase[next_bin+1];
			}
#ifdef OLD		
			// insert any missing points in track
			for (int i = 0; i < t->gap; i++) {
				track_point_t tpg = {0};
				double n = t->gap+1;
				int w2 = i + 1;
				int w1 = n - w2;
				//dp(0, "w1=%d w2=%d n=%g\n", w1, w2, n);
				tpg.bin = (w1*last_track_point.bin + w2*current_peak.bin) / n;
				tpg.power[1] = (w1*last_track_point.power[1] + w2*current_peak.power[1]) / n;
				tpg.phase[1] = (w1*last_track_point.phase[1] + w2*current_peak.phase[1]) / n;
				g_array_append_val(t->points, tpg); 
			}
#endif
			g_array_append_val(t->points, current_peak);
			t->gap = 0;
		} else {
			t->completed = 1;
			if (t->points->len >= min_track_length)
				g_array_append_val(completed_tracks, *t);
			else
				g_array_free(t->points, 1);
			g_array_remove_index_fast(active_tracks, i);
			i--;
		}
	}
	// start a new sinusoid
	for (int j = 0; j < n_peaks; j++) {
		// FIXME handle adjoining peaks better
		int bin = peaks[j];
		if (peak_used_in_track[bin])
			continue;
		if (j && is_peak[bin-1])
			continue;
		track_t t = {0};
		t.fft = fft;
		t.start = step;
		t.points = g_array_new(0, 1, sizeof (track_point_t));
		t.last_peak_power = power[bin];
		track_point_t current_peak = {0};
		current_peak.bin = bin;
		current_peak.power[0] = power[bin-1];
		current_peak.power[1] = power[bin];
		current_peak.power[2] = power[bin+1];
		if (phase) {
			current_peak.phase[0] = phase[bin-1];
			current_peak.phase[1] = phase[bin];
			current_peak.phase[2] = phase[bin+1];
		}
		g_array_append_val(t.points, current_peak);
		g_array_append_val(active_tracks, t);
		dp(22, "new track starts at (%d)\n", current_peak.bin);
	}
	dp(21, "completed_tracks->len=%d active_tracks->len=%d \n", completed_tracks->len, active_tracks->len);
	return completed_tracks;
}

GArray *
update_sinusoid_tracks(fft_t fft, power_t power_spectrum[restrict fft.n_bins], phase_t phase[restrict fft.n_bins], GArray *active_tracks, int step, int approximate) {
	GArray *completed_tracks = g_array_new(0, 1, sizeof (track_t));
	sinusoid_t current_sinusoids[fft.n_bins];
	int n_current_sinusoids = find_sinusoids(fft.n_bins, power_spectrum,  phase, 0, approximate, current_sinusoids);
	uint32_t sinusoid_used_in_track[n_current_sinusoids];
	memset(sinusoid_used_in_track,0, sizeof sinusoid_used_in_track);
	double seconds_per_step = fft.step_size/fft.sampling_rate;
	double max_frequency_delta = param_get_double("spectral_analysis", "max_frequency_delta") * seconds_per_step;
	int min_track_length = 0.5+param_get_double("spectral_analysis", "min_track_length")/seconds_per_step;
	int max_gap_size = 0.5+param_get_double("spectral_analysis", "max_gap_size")/seconds_per_step;;
	dp(22, "max_frequency_delta=%g max_gap_size=%d min_track_length=%d active_tracks=%d\n", (double)max_frequency_delta, (int)max_gap_size, (int)min_track_length, (int)active_tracks->len); 
	
	for (int i = 0; i < active_tracks->len; i++) {
		track_t *t = &g_array_index(active_tracks, track_t, i);
		if (t->completed)
			continue;
		sinusoid_t last_sinusoid = g_array_index(t->points, sinusoid_t, t->points->len-1);
		dp(25, "i=%d last_sinusoid.frequency=%g\n", i, last_sinusoid.frequency);
		double best_distance = 10e10;
		int best = -1;
		for (int j = 0; j < n_current_sinusoids; j++) {
			sinusoid_t c = current_sinusoids[j];
			if (sinusoid_used_in_track[j])
				continue;
			double frequency_ratio = last_sinusoid.frequency > c.frequency ? last_sinusoid.frequency/c.frequency : c.frequency/last_sinusoid.frequency;
			if (frequency_ratio > max_frequency_delta)
				continue;
			double amplitude_ratio = last_sinusoid.amplitude > c.amplitude ? last_sinusoid.amplitude/c.amplitude : c.amplitude/last_sinusoid.amplitude;
			double d = 0.9*frequency_ratio + 0.1*amplitude_ratio;
//			dp(25, "distance(%g,%g,%g,%g)->%g\n", last_sinusoid.frequency, last_sinusoid.amplitude, c.frequency, c.amplitude, d);
			if (d < best_distance) {
				best_distance = d;
				best = j;
			}
		}
		if (best == -1) {
			t->gap++;
			t->completed = t->gap > max_gap_size;
			if (t->completed) {
				if (t->points->len >= min_track_length)
					g_array_append_val(completed_tracks, *t);
				else
					g_array_free(t->points, 1);
				g_array_remove_index_fast(active_tracks, i);
				i--;
			}
		} else {
			sinusoid_used_in_track[best] = 1;
			// insert any missing points in track
			sinusoid_t current_sinusoid = current_sinusoids[best];
			for (int i = 0; i < t->gap; i++) {
				sinusoid_t s = {0};
				double n = t->gap+1;
				int w2 = i + 1;
				int w1 = n - w2;
				//dp(0, "w1=%d w2=%d n=%g\n", w1, w2, n);
				s.frequency = (w1*last_sinusoid.frequency + w2*current_sinusoid.frequency) / n;
				s.amplitude = (w1*last_sinusoid.amplitude + w2*current_sinusoid.amplitude) / n;
				s.phase = (w1*last_sinusoid.phase + w2*current_sinusoid.phase) / n;
				g_array_append_val(t->points, s); 
			}
			g_array_append_val(t->points, current_sinusoid);
			t->gap = 0;
			dp(22, "track[%d] += (%g,%g,%g)\n", i, current_sinusoid.frequency, current_sinusoid.amplitude, current_sinusoid.phase);
		}
	}
	// start a new sinusoid
	for (int j = 0; j < n_current_sinusoids; j++) {
		if (sinusoid_used_in_track[j])
			continue;
		track_t t = {0};
		t.fft = fft;
		t.start = step;
		t.points = g_array_new(0, 1, sizeof (sinusoid_t));
		sinusoid_t s = current_sinusoids[j];
		g_array_append_val(t.points, s);
		g_array_append_val(active_tracks, t);
		dp(22, "new track starts at (%g,%g,%g)\n", s.frequency, s.amplitude, s.phase);
	}
	return completed_tracks;
}

int
peaks_to_sinusoids(int n_peaks, int peaks[n_peaks], int n_bins, power_t power[n_bins], phase_t phase[n_bins], int approximate, sinusoid_t sinusoids[n_bins]) {
	int n_sinusoids = 0;
	for (int i = 0; i < n_peaks; i++) {
		if (i && peaks[i-1] == peaks[i] - 1)
			continue;
		sinusoids[n_sinusoids++] = estimate_sinusoid_parameters(peaks[i], n_bins, power, phase, approximate);
	}
	return n_sinusoids;
}

int
find_sinusoids(int n_bins, power_t power[n_bins], phase_t phase[n_bins], int bin_offset, int approximate, sinusoid_t sinusoids[n_bins]) {
	int peaks[n_bins];
	int n_peaks = bins_to_peaks(n_bins, power, peaks);
	return peaks_to_sinusoids(n_peaks, peaks, n_bins, power, phase, approximate, sinusoids);
}
