#include "i.h"

#define VERSION "0.1.1"

void
sound_to_image(char *sound_file, char *image_file) {
	soundfile_t	*infile = soundfile_open_read(sound_file);
	if (!infile) sdie(NULL, "can not open input file %s: ", sound_file) ;
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
	double (*log_power0)[fft.n_bins] = salloc(n_steps*fft.n_bins*sizeof log_power0[0][0]);
	double (*log_power1)[fft.n_bins] = salloc(n_steps*fft.n_bins*sizeof log_power1[0][0]);
	double (*tracks)[fft.n_bins] = salloc(n_steps*fft.n_bins*sizeof tracks[0][0]);
	GArray *active_tracks[n_channels];
	sample_t samples_buffer[fft.window_size][n_channels];
	for (int channel = 0; channel < n_channels; channel++)
		active_tracks[channel] = g_array_new(0, 1, sizeof (track_t));
	g_array_new(0, 1, sizeof (track_t));
	for (index_t step = 0; step < n_steps; step++) {
		if (step == 0) {
			if (soundfile_read(infile, (sample_t *)samples_buffer, fft.window_size) !=  fft.window_size)
				die("soundfile_read_double returned insufficient frames");
		} else {
			for (int i = fft.step_size; i < fft.window_size; i++)
				for (int channel = 0; channel <  n_channels; channel++)
					samples_buffer[i - fft.step_size][channel] = samples_buffer[i][channel];
				
			if (soundfile_read(infile, (sample_t *)&samples_buffer[fft.step_size], fft.window_size-fft.step_size) !=  fft.window_size-fft.step_size)
				die("soundfile_read_double returned insufficient frames");
		}
		for (int channel = 0; channel < MIN(n_channels, 2); channel++) {
			double (*log_power)[fft.n_bins] = channel ? log_power1 : log_power0;
			sample_t mono[fft.window_size];
			for (int sample = 0; sample < fft.window_size; sample++)
				mono[sample] = samples_buffer[sample][channel];
			power_t power[fft.n_bins];
			phase_t phase[fft.n_bins];
			short_time_power_phase(mono, &fft, (void *)power, (void *)phase); //yuk
			for (int i = 0; i < fft.n_bins; i++)
				log_power[step][i] = log(double_to_power_t(power[i]));
			GArray *completed_tracks = update_sinusoid_tracks(fft, power, phase, active_tracks[channel], step, 0);
			for (int j = 0; j < completed_tracks->len; j++) {
				track_t *t = &g_array_index(completed_tracks, track_t, j);
				for (int k = 0; k < t->points->len; k++) {
					sinusoid_t *s = &g_array_index(t->points, sinusoid_t, k);
					dp(26, "k=%d frequency=%g bin=%d\n", k, s->frequency, (int)(s->frequency*fft.n_bins*2+0.5));
					int x = step - (t->points->len - (k+1));
					int y = s->frequency*fft.n_bins*2+0.5;
					tracks[x][y] = 1;
					log_power0[x][y] = 0;
					log_power1[x][y] = 0;
				}
				g_array_free(g_array_index(completed_tracks, track_t, j).points, 1);
			}
			g_array_free(completed_tracks,1);
		}
	}
	write_arrays_as_rgb_jpg(image_file, n_steps, fft.n_bins, tracks, log_power0, log_power1, 0, 1, NULL);
}

int
main(int argc, char *argv[])
{
	int optind = initialize(argc, argv, VERSION, "<sound files>");
	if (argc - optind != 2)
		die("Usage: <sound file> <image file>");
	sound_to_image(argv[optind], argv[optind + 1]);
	return 0;
}

