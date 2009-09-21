#include "i.h"

#define VERSION "0.1.1"

int
main(int argc, char *argv[]) 
{
	int optind = initialize(argc, argv, SPECTRAL_ANALYSIS_GROUP, VERSION, "<input-sound-file> <output-sound-file>");
	if (argc - optind != 2)
		die("Usage: <input-sound-file> <output-sound-file>\n");
	silence_removal_file(argv[optind], argv[optind+1]);
	return 0;
}

void
silence_removal_file(char *infilename, char *outfilename) {
	soundfile_t	*infile = soundfile_open_read(infilename);
	if (!infile) sdie(NULL, "can not open input file %s: ", infilename) ;
	soundfile_t	*outfile = soundfile_open_write(outfilename, infile->channels, infile->samplerate);
	if (!outfile) sdie(NULL, "can not open output file %s: ", outfilename) ;
	int n_channels = infile->channels;
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
	double seconds_per_step = fft.step_size/fft.sampling_rate;
	int prefix_frames = 0.5+param_get_double("call", "prefix_seconds")/seconds_per_step;
	int suffix_frames = 0.5+param_get_double("call", "suffix_seconds")/seconds_per_step;
	GArray *past_samples = g_array_new(0, 1, sizeof (sample_t **));
	GArray *past_power = g_array_new(0, 1, sizeof (power_t **));
	GArray *silence = g_array_new(0, 1, sizeof (int));
	GArray *active_tracks[n_channels];
	sample_t samples_buffer[fft.window_size][n_channels];
	for (int channel = 0; channel < n_channels; channel++)
		active_tracks[channel] = g_array_new(0, 1, sizeof (track_t));
	g_array_new(0, 1, sizeof (track_t));
	int steps_since_completed_track = suffix_frames+1;
	for (index_t step = 0; step < n_steps; step++) {
		if (step == 0) {
			if (soundfile_read(infile, (sample_t *)samples_buffer, fft.window_size) !=  fft.window_size)
				die("soundfile_read returned insufficient frames");
		} else {
			for (int i = fft.step_size; i < fft.window_size; i++)
				for (int channel = 0; channel <  n_channels; channel++)
					samples_buffer[i - fft.step_size][channel] = samples_buffer[i][channel];
				
			if (soundfile_read(infile, (sample_t *)&samples_buffer[fft.step_size][0], fft.window_size-fft.step_size) !=  fft.window_size-fft.step_size)
				die("soundfile_read returned insufficient frames");
		}
		int maximum_active_track_length = 0;
		power_t **power = g_slice_alloc(n_channels*sizeof (power_t *));
		dp(31, "power=%p\n", power);
		g_array_insert_val(past_power, 0, power);
		sample_t **samples = g_slice_alloc(n_channels*sizeof (sample_t *));
		g_array_insert_val(past_samples, 0, samples);
		int true = 1;
		g_array_insert_val(silence, 0, true);
		for (int channel = 0; channel < n_channels; channel++) {
			sample_t mono[fft.window_size];
			for (int sample = 0; sample < fft.window_size; sample++)
				mono[sample] = samples_buffer[sample][channel];
			samples[channel] = g_slice_alloc(fft.step_size*sizeof samples[0][0]);
			int new_samples_index = fft.window_size-fft.step_size;
			for (int sample = new_samples_index; sample < fft.window_size; sample++)
				samples[channel][sample-new_samples_index] = samples_buffer[sample][channel];
			power[channel] = g_slice_alloc(fft.n_bins*sizeof power[0][0]);
			phase_t phase[fft.n_bins];
			short_time_power_phase(mono, &fft, (void *)power[channel], (void *)phase); //yuk
			GArray *completed_tracks = update_sinusoid_tracks(fft, power[channel], phase, active_tracks[channel], step, 1);
			dp(21, "step=%d channel=%d completed_tracks->len=%d\n", step, channel, completed_tracks->len);
			for (int j = 0; j < completed_tracks->len; j++) {
				track_t *t = &g_array_index(completed_tracks, track_t, j);
				for (int i = 0; i < t->points->len + prefix_frames&& i < silence->len; i++)
					g_array_index(silence, int, i) = 0;
				g_array_free(t->points, 1);
				steps_since_completed_track = 0;
			}
			g_array_free(completed_tracks,1);
			for (int j = 0; j < active_tracks[channel]->len; j++)
				maximum_active_track_length = MAX(maximum_active_track_length, g_array_index(active_tracks[channel], track_t, j).points->len);
			dp(23, "maximum_active_track_length=%d\n", maximum_active_track_length);
		}
		if (steps_since_completed_track++ < suffix_frames)
			g_array_index(silence, int, 0) = 0;
			
		while (past_power->len > maximum_active_track_length+prefix_frames) {
			power_t **power =  g_array_index(past_power, power_t **, past_power->len-1);
			sample_t **samples =  g_array_index(past_samples, sample_t **, past_samples->len-1);
			sample_t buffer[n_channels*fft.step_size];
			if (g_array_index(silence, int, silence->len-1)) {
				dp(23, "step %d: writing silence for step %d\n", step, step-past_power->len-1);
				memset(buffer, 0, sizeof buffer);
			} else {
				dp(23, "step %d: writing sound for step %d\n", step, step-past_power->len-1);
				for (int channel = 0;  channel < n_channels; channel++)
					for (int i = 0; i < fft.step_size; i++)
						buffer[i*n_channels+channel] = samples[channel][i];
			}
			soundfile_write(outfile, buffer, fft.step_size);
			for (int channel = 0; channel < n_channels; channel++)
				g_slice_free1(fft.n_bins*sizeof power[0][0], power[channel]);
			for (int channel = 0; channel < n_channels; channel++)
				g_slice_free1(fft.step_size*sizeof samples[0][0], samples[channel]);
			g_array_remove_index_fast(past_power, past_power->len-1);
			g_array_remove_index_fast(past_samples, past_samples->len-1);
			g_array_remove_index_fast(silence, silence->len-1);
		}
		
	}
	while (past_power->len > 0) {
		power_t **power =  g_array_index(past_power, power_t **, past_power->len-1);
		sample_t **samples =  g_array_index(past_samples, sample_t **, past_samples->len-1);
		sample_t buffer[n_channels*fft.step_size];
		if (g_array_index(silence, int, silence->len-1)) {
			dp(23, "finish writing silence for step %d\n", n_steps - 1 - past_power->len);
			memset(buffer, 0, sizeof buffer);
		} else {
			dp(23, "finish writing sound for step %d\n", n_steps - 1 - past_power->len);
			for (int channel = 0;  channel < n_channels; channel++)
				for (int i = 0; i < fft.step_size; i++)
					buffer[i*n_channels+channel] = samples[channel][i];
		}
		soundfile_write(outfile, buffer, fft.step_size);
		for (int channel = 0; channel < n_channels; channel++)
			g_slice_free1(fft.n_bins*sizeof power[0][0], power[channel]);
		for (int channel = 0; channel < n_channels; channel++)
			g_slice_free1(fft.step_size*sizeof samples[0][0], samples[channel]);
		g_array_remove_index_fast(past_power, past_power->len-1);
		g_array_remove_index_fast(past_samples, past_samples->len-1);
		g_array_remove_index_fast(silence, silence->len-1);
	}
	// FIXME free remaining active tracks & their points
	soundfile_close(infile);
	soundfile_close(outfile);
}
