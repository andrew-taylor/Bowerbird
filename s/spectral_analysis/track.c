#include "i.h"

void
extract_calls_file(char *filename, FILE *index_file, int *call_count) {
	char *peaks_image_filename_format = param_get_string_n("call", "peaks_image_filename");
	int calculate_phase = param_get_integer("call", "calculate_phase");
	soundfile_t	*infile = soundfile_open_read(filename);
	if (!infile) sdie(NULL, "can not open input file %s: ", filename) ;
	int n_channels = infile->channels;
	uint64_t ignore_channel_bitmap = param_get_integer("call", "ignore_channel_bitmap");
	fft_t fft = {0};
	fft.window_size = param_get_integer("spectral_analysis", "fft_window");
	fft.sampling_rate = infile->samplerate;
	fft.fft_size = param_get_integer("spectral_analysis", "fft_points");
	fft.step_size = (1 - param_get_double("spectral_analysis", "fft_overlap")) * fft.window_size;
	dp(2, "window_size=%d fft_size=%d step_size=%d\n", fft.window_size, fft.fft_size, fft.step_size);
	fft.window_type = fw_hann;
	fft.n_bins = (fft.fft_size+1)/2;
	fft.n_steps = 1;
	int n_steps = 1+(infile->frames-fft.window_size)/fft.step_size;
	if (infile->frames < fft.window_size)
		n_steps = 0;
	char *output_directory = param_get_string("call", "output_directory");
	char *prefix = param_sprintf("call", "pathname_prefix", output_directory);
	g_free(output_directory);
	char *call_database = param_sprintf("call", "database", prefix);
	sqlite3_stmt *sql_statement = NULL;
#ifdef USE_SQLITE
	sqlite3 *sql_db = NULL;
	if (strlen(call_database)) {
		if (sqlite3_open(call_database, &sql_db))
			die("Can't open database %s: %s\n", call_database, sqlite3_errmsg(sql_db));
		char *err = 0;
		sqlite3_busy_timeout(sql_db, param_get_integer("database", "busy_timeout"));
		char *start_cmd = param_get_string("database", "start_cmd");
		if(sqlite3_exec(sql_db, start_cmd, 0, 0, &err) != SQLITE_OK)
			die("SQL error from '%s' -> %s\n", start_cmd, err);
		if (sqlite3_prepare_v2(sql_db, param_get_string("database", "source_insert"), -1, &sql_statement, NULL) != SQLITE_OK)
			die("SQL error from sqlite3_prepare_v2: %s\n", sqlite3_errmsg(sql_db));
		// FIXME replace column numbers with constants
		sqlite3_bind_text(sql_statement, 1, filename, -1, SQLITE_STATIC);
		sqlite3_bind_double(sql_statement, 2, fft.sampling_rate);
		sqlite3_bind_int(sql_statement, 3, (int)fft.n_bins);
		sqlite3_bind_int(sql_statement, 4, (int)fft.step_size);
		sqlite3_bind_int(sql_statement, 5, (int)fft.window_size);
		if (sqlite3_step(sql_statement) != SQLITE_DONE)
			die("SQL error from sqlite3_step inserting source %s: %s\n", filename, sqlite3_errmsg(sql_db));
		dp(20, "source '%s' inserted\n", filename);
		if (sqlite3_prepare_v2(sql_db, param_get_string("database", "unit_insert"), -1, &sql_statement, NULL) != SQLITE_OK)
			die("SQL error from sqlite3_prepare_v2: %s\n", sqlite3_errmsg(sql_db));
		sqlite3_bind_int64(sql_statement, 1, sqlite3_last_insert_rowid(sql_db));
	}
#endif
	GArray *past_samples = g_array_new(0, 1, sizeof (sample_t **));
	GArray *past_power = g_array_new(0, 1, sizeof (power_t **));
	GArray *past_peaks = peaks_image_filename_format ? g_array_new(0, 1, sizeof (int **)) : NULL;
	GArray *track_history = peaks_image_filename_format ? g_array_new(0, 1, sizeof (track_t)) : NULL;
	GArray *active_tracks[n_channels];
	uint32_t min_track_length = 0.5+param_get_double("spectral_analysis", "min_track_length")/(fft.step_size/fft.sampling_rate);
	sample_t samples_buffer[fft.window_size][n_channels];
	for (int channel = 0; channel < n_channels; channel++)
		active_tracks[channel] = g_array_new(0, 1, sizeof (track_t));
	g_array_new(0, 1, sizeof (track_t));
	for (index_t step = 0; step < n_steps; step++) {
		dp(22, "step=%d\n", (int)step);
		if (step == 0) {
			if (soundfile_read(infile, (sample_t *)samples_buffer, fft.window_size) !=  fft.window_size)
				die("soundfile_read returned insufficient frames");
		} else {
			for (int i = fft.step_size; i < fft.window_size; i++)
				for (int channel = 0; channel <  n_channels; channel++)
					samples_buffer[i - fft.step_size][channel] = samples_buffer[i][channel];
				
			if (soundfile_read(infile, (sample_t *)&samples_buffer[fft.window_size - fft.step_size], fft.step_size) !=  fft.step_size)
				die("soundfile_read returned insufficient frames");
		}
		int maximum_track_length = 1;
		power_t **power = g_slice_alloc(n_channels*sizeof (power_t *));
		dp(31, "power=%p\n", power);
		g_array_insert_val(past_power, 0, power);
		int **peaks = NULL;
		if (past_peaks) {
			peaks = g_slice_alloc(n_channels*sizeof (int *));
			g_array_insert_val(past_peaks, 0, peaks);
		}
		sample_t **samples = g_slice_alloc(n_channels*sizeof (sample_t *));
		g_array_insert_val(past_samples, 0, samples);
		for (int channel = 0; channel < n_channels; channel++) {
			if (ignore_channel_bitmap & (1 << channel))
				continue;
			sample_t mono[fft.window_size];
			for (int sample = 0; sample < fft.window_size; sample++)
				mono[sample] = samples_buffer[sample][channel];
			samples[channel] = g_slice_alloc(fft.step_size*sizeof samples[0][0]);
			int new_samples_index = fft.window_size-fft.step_size;
			for (int sample = new_samples_index; sample < fft.window_size; sample++)
				samples[channel][sample-new_samples_index] = samples_buffer[sample][channel];
			power[channel] = g_slice_alloc(fft.n_bins*sizeof power[0][0]);
			phase_t phase1[fft.n_bins];
			phase_t *phase = calculate_phase ? phase1 : NULL;
			short_time_power_phase(mono, &fft, (void *)power[channel], (void *)phase); //yuk
			power_t *previous_power = step ?  (g_array_index(past_power, power_t **, 1))[channel] : NULL; 
			GArray *completed_tracks = new_update_sinusoid_tracks(fft, power[channel], previous_power, phase, active_tracks[channel], step);
			if (peaks) {
				peaks[channel] = g_slice_alloc((fft.n_bins+1)*sizeof peaks[0][0]);
				int n_peaks = bins_to_peaks(fft.n_bins, power[channel], peaks[channel]);
				peaks[channel][n_peaks] = -1;
			}
			dp(26, "completed_tracks->len=%d active_tracks[channel]->len=%d \n", completed_tracks->len, active_tracks[channel]->len);
			for (int j = 0; j < completed_tracks->len; j++) {
				process_track(&g_array_index(completed_tracks, track_t, j), fft, filename, channel, step, past_power, past_samples, index_file, call_count, sql_statement);
				if (track_history) {
					g_array_append_val(track_history, g_array_index(completed_tracks, track_t, j));
				} else {	
					g_array_free(g_array_index(completed_tracks, track_t, j).points, 1);
				}
			}
			g_array_free(completed_tracks,1);
			for (int j = 0; j < active_tracks[channel]->len; j++)
				maximum_track_length = MAX(maximum_track_length, g_array_index(active_tracks[channel], track_t, j).points->len);
		}
		while (past_power->len > maximum_track_length && !peaks_image_filename_format) {
			power_t **power =  g_array_index(past_power, power_t **, past_power->len-1);
			sample_t **samples =  g_array_index(past_samples, sample_t **, past_samples->len-1);
			for (int channel = 0; channel < n_channels; channel++) {
				if (ignore_channel_bitmap & (1 << channel))
					continue;
				g_slice_free1(fft.n_bins*sizeof power[0][0], power[channel]);
				g_slice_free1(fft.step_size*sizeof samples[0][0], samples[channel]);
			}
			g_array_remove_index_fast(past_power, past_power->len-1);
			g_array_remove_index_fast(past_samples, past_samples->len-1);
		}
		
	}
	for (int channel = 0; channel < n_channels; channel++) {
		if (ignore_channel_bitmap & (1 << channel))
			continue;
		for (int j = 0; j < active_tracks[channel]->len; j++) {
			track_t *t = &g_array_index(active_tracks[channel], track_t, j);
			int track_ok = t->completed || t->points->len >= min_track_length;
			if (track_ok)
				process_track(t, fft, filename, channel, n_steps, past_power, past_samples, index_file, call_count, sql_statement);
			if (track_history && track_ok) {
				g_array_append_val(track_history, *t);
			} else {	
				g_array_free(t->points, 1);
			}
		}
	}
	if (peaks_image_filename_format) {
		int buffer_size = param_get_integer("call", "peaks_image_maximum_length");
		int needed = MIN(n_steps, buffer_size);
		dp(26, "needed=%d n_steps=%d, buffer_size=%d\n", needed, n_steps, buffer_size);
		double (*log_power)[fft.n_bins] = salloc(needed*fft.n_bins*sizeof log_power[0][0]);
		double (*peaks)[fft.n_bins] = salloc(needed*fft.n_bins*sizeof peaks[0][0]);
		double (*tracks)[fft.n_bins] = salloc(needed*fft.n_bins*sizeof tracks[0][0]);
		for (int channel = 0; channel < n_channels; channel++) {
			if (ignore_channel_bitmap & (1 << channel))
				continue;
			for (int image_count = 0; image_count < (n_steps-1)/buffer_size+1; image_count++) {
				int step_offset= image_count*buffer_size;
				char *peaks_image_filename = g_strdup_printf(peaks_image_filename_format, prefix, channel, image_count);
				memset(log_power, 0, needed*fft.n_bins*sizeof log_power[0][0]);
				memset(peaks, 0, needed*fft.n_bins*sizeof peaks[0][0]);
				memset(tracks, 0, needed*fft.n_bins*sizeof tracks[0][0]);
				for (int step = 0; step < MIN(needed, n_steps-step_offset); step++) {
					int s = step_offset+step;
					// dp(1, "s=%d step=%d image_count=%d\n", s, step, image_count);
					power_t **power =  g_array_index(past_power, power_t **, n_steps-s-1);  
					for (int i = 0; i < fft.n_bins; i++) {
						if (power[channel][i])
							log_power[step][i] = log(power[channel][i]);
					}
					int **p =  g_array_index(past_peaks, int **,  n_steps-s-1);  
					for (int i = 0; i < fft.n_bins; i++) {
						if (p[channel][i] <= 0)
							break;
						peaks[step][p[channel][i]] = 8;
						log_power[step][p[channel][i]] = 0;
					}
				}
				for (int j = 0; j < track_history->len; j++) {
					track_t *t = &g_array_index(track_history, track_t, j);
					if (!t->completed && t->points->len < min_track_length)
						continue;
					if (t->start >= step_offset + needed || t->start + t->points->len < step_offset)
						continue;
					dp(31, "t->start=%d\n", t->start);
					int min_bin = fft.n_bins - 1;
					int max_bin = 0;
					for (int k = 0; k < t->points->len; k++) {
						track_point_t *s = &g_array_index(t->points, track_point_t, k);
						min_bin = MIN(s->bin, min_bin);
						max_bin = MAX(s->bin, max_bin);
					}
					// min_bin = MAX(0, min_bin - 8);
					// max_bin = MAX(0, max_bin + 8);
					for (int k = 0; k < t->points->len; k++) {
						int m = t->start + k - step_offset;
						// dp(1, "m=%d t->start=%d k=%d\n", m, t->start, k);
						if (m >= 0 && m < buffer_size) {
							track_point_t *s = &g_array_index(t->points, track_point_t, k);
							tracks[m][s->bin] = 1;
							peaks[m][s->bin] = (j&2)*2 +(j&8);
							log_power[m][s->bin] = (j&1)*2 + (j&4);
							if (k == 0 || k == t->points->len - 1) {
								for (int b = min_bin; b <= max_bin; b++) {
									if (b == s->bin) continue;
									tracks[m][b] = 0;
									peaks[m][b] = 0;
									log_power[m][b] = 0;
								}
							} else {
								if (min_bin != s->bin) {
									tracks[m][min_bin] = 0;
									peaks[m][min_bin] = 0;
									log_power[m][min_bin] = 0;
								}
								if (max_bin != s->bin) {
									tracks[m][max_bin] = 0;
									peaks[m][max_bin] = 0;
									log_power[m][max_bin] = 0;
								}
							}
						}
					}
				}
				write_arrays_as_rgb_jpg(peaks_image_filename, needed, fft.n_bins, tracks, peaks, log_power, 0, 1, NULL);
				g_free(peaks_image_filename);
			}
		}
		g_free(log_power);
		g_free(peaks);
		g_free(tracks);
	}
#ifdef USE_SQLITE
	if (sql_statement) {
		char *err = 0;
		if(sqlite3_exec(sql_db, param_get_string("database", "finish_cmd"), 0, 0, &err) != SQLITE_OK)
			die("SQL error from finish_cmd: %s\n", err);
		sqlite3_finalize(sql_statement);
		sqlite3_close(sql_db);
	}
#endif
	while (past_power->len > 0) {
		power_t **power =  g_array_index(past_power, power_t **, past_power->len-1);
		sample_t **samples =  g_array_index(past_samples, sample_t **, past_samples->len-1);
		for (int channel = 0; channel < n_channels; channel++) {
			if (ignore_channel_bitmap & (1 << channel))
				continue;
			g_slice_free1(fft.n_bins*sizeof power[0][0], power[channel]);
			g_slice_free1(fft.step_size*sizeof samples[0][0], samples[channel]);
		}
		g_array_remove_index_fast(past_power, past_power->len-1);
		g_array_remove_index_fast(past_samples, past_samples->len-1);
	}
	soundfile_close(infile);
	g_free(prefix);
}

void
score_calls_file(char *filename) {
	int calculate_phase = 0;
	soundfile_t	*infile = soundfile_open_read(filename);
	if (!infile) sdie(NULL, "can not open input file %s: ", filename) ;
	int n_channels = infile->channels;
	uint64_t ignore_channel_bitmap = param_get_integer("call", "ignore_channel_bitmap");
	fft_t fft = {0};
	fft.window_size = param_get_integer("spectral_analysis", "fft_window");
	fft.sampling_rate = infile->samplerate;
	fft.fft_size = param_get_integer("spectral_analysis", "fft_points");
	fft.step_size = (1 - param_get_double("spectral_analysis", "fft_overlap")) * fft.window_size;
	fft.window_type = fw_hann;
	fft.n_bins = (fft.fft_size+1)/2;
	fft.n_steps = 1;
	int n_steps = 1+(infile->frames-fft.window_size)/fft.step_size;
	if (infile->frames < fft.window_size)
		n_steps = 0;
	GArray *active_tracks[n_channels];
	sample_t samples_buffer[fft.window_size][n_channels];
	for (int channel = 0; channel < n_channels; channel++)
		active_tracks[channel] = g_array_new(0, 1, sizeof (track_t));
	g_array_new(0, 1, sizeof (track_t));
	double sum_score = 0, max_score = 0;
	int n_tracks = 0;
	for (index_t step = 0; step < n_steps; step++) {
		if (step == 0) {
			if (soundfile_read(infile, (sample_t *)samples_buffer, fft.window_size) !=  fft.window_size)
				die("soundfile_read returned insufficient frames");
		} else {
			for (int i = fft.step_size; i < fft.window_size; i++)
				for (int channel = 0; channel <  n_channels; channel++)
					samples_buffer[i - fft.step_size][channel] = samples_buffer[i][channel];
				
			if (soundfile_read(infile, (sample_t *)&samples_buffer[fft.window_size - fft.step_size], fft.step_size) !=  fft.step_size)
				die("soundfile_read returned insufficient frames");
		}
		for (int channel = 0; channel < n_channels; channel++) {
			if (ignore_channel_bitmap & (1 << channel))
				continue;
			sample_t mono[fft.window_size];
			for (int sample = 0; sample < fft.window_size; sample++)
				mono[sample] = samples_buffer[sample][channel];
			power_t power[2][fft.n_bins];
			phase_t phase[1][fft.n_bins];
			short_time_power_phase(mono, &fft, &power[step % 2], calculate_phase ? phase : NULL); 
			GArray *completed_tracks = new_update_sinusoid_tracks(fft, power[step % 2], power[step % 2 + 1], phase[0], active_tracks[channel], step);
			dp(31, "completed_tracks->len=%d\n", completed_tracks->len);
			for (int j = 0; j < completed_tracks->len; j++) {
				double score = score_track(&g_array_index(completed_tracks, track_t, j));
				max_score = MAX(max_score, score);
				sum_score += score;
				n_tracks++;
				g_array_free(g_array_index(completed_tracks, track_t, j).points, 1);
			}
			g_array_free(completed_tracks,1);
		}
	}
	for (int channel = 0; channel < n_channels; channel++) {
		if (ignore_channel_bitmap & (1 << channel))
			continue;
		for (int j = 0; j < active_tracks[channel]->len; j++) {
			double score = score_track(&g_array_index(active_tracks[channel], track_t, j));
			max_score = MAX(max_score, score);
			sum_score += score;
			n_tracks++;
			g_array_free(g_array_index(active_tracks[channel], track_t, j).points, 1);
		}
	}
	soundfile_close(infile);
	printf("%s %d %g %g\n", filename, n_tracks, sum_score, max_score);
}

void
process_track(track_t *t, fft_t fft, char *source, int channel, index_t step, GArray *past_power, GArray *past_samples,  FILE *index_file, int *call_count, sqlite3_stmt *sql_statement) {
	uint32_t length = t->points->len;
	char *output_directory = param_get_string("call", "output_directory");
	char *prefix = param_sprintf("call", "pathname_prefix", output_directory);
	g_free(output_directory);
	char *image_filename = param_sprintf("call", "image_filename", prefix, *call_count);
	char *details_filename = param_sprintf("call", "details_filename", prefix, *call_count);
	char *track_filename = param_sprintf("call", "track_filename", prefix, *call_count);
	char *sound_filename = param_sprintf("call", "sound_filename", prefix, *call_count);
	char *spectrum_filename = param_sprintf("call", "spectrum_filename", prefix, *call_count);
	g_free(prefix);
	int refine_sinusoid_parameters = param_get_integer("call", "refine_sinusoid_parameters");
	if (index_file)
		fprintf(index_file, "<h6>Call %d<h6>\n", *call_count);
	double (*log_power)[fft.n_bins] = NULL;
	if (image_filename||spectrum_filename) {
		log_power = salloc(length*fft.n_bins*sizeof log_power[0][0]);
		for (int step = 0; step < length; step++) {
			power_t **power =  g_array_index(past_power, power_t **, step);  
			for (int i = 0; i < fft.n_bins; i++) {
				if (power[channel][i])
					log_power[length-step-1][i] = log(power[channel][i]);
			}
		}
	}
	if (image_filename) {
		double (*tracks)[fft.n_bins] = salloc(length*fft.n_bins*sizeof tracks[0][0]);
		for (int k = 0; k < t->points->len; k++) {
			track_point_t *s = &g_array_index(t->points, track_point_t, k);
			dp(26, "k=%d bin=%d\n", k, s->bin);
			tracks[k][s->bin] = 1;
		}
		write_arrays_as_rgb_jpg(image_filename, length, fft.n_bins, tracks, NULL, log_power, 1, 0, param_get_string("call", "image_size"));
		if (index_file)
			fprintf(index_file, "<img src=\"%s\">\n", image_filename);
		g_free(tracks);
		g_free(image_filename);
	}
	if (index_file)
		fprintf(index_file, "<pre>\n");
	index_t track_start_samples = (step - length)*fft.step_size;
	index_t track_len_samples = length*fft.step_size;
	dp(10, "track found - start frame %d length %d frames\n", track_start_samples, track_len_samples);
	if (details_filename) {
		FILE *details_file = fopen(details_filename, "w");
		assert(details_file);
		fprintf(details_file, "source=%s\nsampling_rate=%g\nchannel=%d\nfirst_frame=%ld\nn_frames=%ld\n", source, fft.sampling_rate, channel, (long)track_start_samples, (long)track_len_samples);
		fprintf(details_file, "fft_n_bins=%d\nfft_step_size=%d\nfft_window_size=%d\n", (int)fft.n_bins, (int)fft.step_size, (int)fft.window_size);
		fclose(details_file);
		g_free(details_filename);
	}
	if (index_file)
		fprintf(index_file, "channel=%d\noffset=%g\nlength=%g\n", channel, track_start_samples/fft.sampling_rate, track_len_samples/fft.sampling_rate);
#ifdef USE_SQLITE
	if (sql_statement) {
		// FIXME replace column numbers with constants
		sqlite3_bind_int(sql_statement, 2, (int)channel);
		sqlite3_bind_int64(sql_statement, 3, track_start_samples);
		sqlite3_bind_int64(sql_statement, 4, track_len_samples);
		double frequency[t->points->len];
		double amplitude[t->points->len];
		double phase[t->points->len];
		sqlite3_bind_blob(sql_statement, 5, frequency, sizeof frequency, SQLITE_STATIC);
		sqlite3_bind_blob(sql_statement, 6, amplitude, sizeof amplitude, SQLITE_STATIC);
		sqlite3_bind_blob(sql_statement, 7, phase, sizeof phase, SQLITE_STATIC);
		for (int k = 0; k < t->points->len; k++) {
			track_point_t *tp = &g_array_index(t->points, track_point_t, k);
			if (!refine_sinusoid_parameters) {
				frequency[k] = tp->bin/(2.0*t->fft.n_bins);
				amplitude[k] = sqrt(1.5*power_t_to_double(tp->power[1])/t->fft.n_bins);
				phase[k] = tp->phase[1];
			} else {
				sinusoid_t s = calculate_refined_sinusoid_parameters(t, tp);
				frequency[k] = s.frequency;
				amplitude[k] = s.amplitude;
				phase[k] = s.phase;
			}
		}
		if (sqlite3_step(sql_statement) != SQLITE_DONE)
			die("SQL error from sqlite3_step inserting unit %d\n", *call_count);
		sqlite3_reset(sql_statement);
	}
#endif

	if (track_filename) {
		FILE *track_file = fopen(track_filename, "w");
		assert(track_file);
		for (int k = 0; k < t->points->len; k++) {
			track_point_t *tp = &g_array_index(t->points, track_point_t, k);
			if (!refine_sinusoid_parameters)
				fprintf(track_file, "%g %g %g\n", tp->bin/(2.0*t->fft.n_bins), sqrt(1.5*power_t_to_double(tp->power[1])/t->fft.n_bins), tp->phase[1]);
			else {
				sinusoid_t s = calculate_refined_sinusoid_parameters(t, tp);
				fprintf(track_file, "%g %g %g\n", s.frequency, s.amplitude, s.phase);
			}
		}
		fclose(track_file);
		g_free(track_filename);
	}
	if (sound_filename) {
		sample_t samples[length*fft.step_size];
		for (int step = 0; step < length; step++) {
			sample_t **s =  g_array_index(past_samples, sample_t **, step);  
			for (int i = 0; i < fft.step_size; i++)
				samples[step*fft.step_size+i] = s[channel][i];
		}
		soundfile_write_entire(sound_filename, length*fft.step_size, 1, fft.sampling_rate, samples);
		g_free(sound_filename);
	}
	if (spectrum_filename) {
		FILE *spectrum_file = fopen(spectrum_filename, "w");
		assert(spectrum_file);
		int spectrum_radius = param_get_integer("call", "spectrum_radius");
		for (int k = 0; k < t->points->len; k++) {
			track_point_t *s = &g_array_index(t->points, track_point_t, k);
			int index = s->bin;
			dp(31, "index = %d\n", index);
			if (index < spectrum_radius || index >= fft.n_bins-spectrum_radius)
				die("can not write spectrum surrounding call (index=%d spectrum_radius=%d  fft.n_bins=%d", index, spectrum_radius, fft.n_bins);
			assert(index >= spectrum_radius);
			assert(index < fft.n_bins-spectrum_radius);
			for (int i = -spectrum_radius;i <= spectrum_radius; i++)
				fprintf(spectrum_file, "%g ", log_power[k][index+i] );
			fprintf(spectrum_file, "\n");
		}
		fclose(spectrum_file);
		g_free(spectrum_file);
		g_free(spectrum_filename);
	}
	if (index_file)
		fprintf(index_file, "</pre>\n");
	(*call_count)++;
	g_free(log_power);
}


double
score_track(track_t *t) {
	double min_frequency, max_frequency, sum_frequency, min_amplitude, max_amplitude, sum_amplitude;
	track_statistics(t, &min_frequency, &max_frequency, &sum_frequency, &min_amplitude, &max_amplitude, &sum_amplitude);
	return (max_frequency/min_frequency)*sum_amplitude/(sum_frequency/t->points->len);
}

void
track_statistics(track_t *t, double *min_frequency, double *max_frequency, double *sum_frequency, double *min_amplitude, double *max_amplitude, double *sum_amplitude) {
	int n_steps = t->points->len;
	assert(n_steps > 1);
	track_point_t *s0 = &g_array_index(t->points, track_point_t, 0);
	uint32_t min_freq = s0->bin;
	uint32_t max_freq = s0->bin;
	uint64_t sum_freq = 0;
	power_t min_amp = s0->power[1];
	power_t max_amp = s0->power[1];;
	double sum_amp = 0;
	for (int step = 0; step < n_steps; step++) {
		track_point_t *tp = &g_array_index(t->points, track_point_t, step);
		min_freq = MIN(min_freq, tp->bin);
		max_freq = MAX(max_freq, tp->bin);
		sum_freq += tp->bin;
		min_amp = MIN(min_amp, tp->power[1]);
		max_amp = MAX(max_amp, tp->power[1]);
		sum_amp += power_t_to_double(tp->power[1]);
	}
	*min_frequency = min_freq * (double)t->fft.sampling_rate;
	*max_frequency = max_freq * (double)t->fft.sampling_rate;
	*sum_frequency = sum_freq * (double)t->fft.sampling_rate;
	*min_amplitude = power_t_to_double(min_amp);
	*max_amplitude = power_t_to_double(max_amp);
	*sum_amplitude = sum_amp;
}

void
print_track_attributes(track_t *t, FILE *index_file, FILE *attribute_file) {
	int n_steps = t->points->len;
	assert(n_steps > 1);
	double sampling_rate = t->fft.sampling_rate;
	double seconds_per_step = t->fft.step_size/sampling_rate;
	double x[n_steps];
	double y[n_steps];
	for (int step = 0; step < n_steps; step++) {
		track_point_t tp = g_array_index(t->points, track_point_t, step);
		x[step] = step*seconds_per_step;
		y[step] = sampling_rate*tp.bin/(t->fft.n_bins*2);
		dp(21, "x[%d]=%g y[%d]=%g\n", step, x[step], step, y[step]);
	}
	double c0, c1, cov00, cov01, cov11, sumsq;
	gsl_fit_linear(x, 1, y, 1, n_steps, &c0, &c1, &cov00, &cov01, &cov11, &sumsq);
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
