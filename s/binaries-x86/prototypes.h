
/* util/general.c */

extern int		verbosity;
extern FILE	*debug_stream;
extern char *myname;
void set_myname(char *argv[]);
#ifndef __GNUC__
#define __attribute__()
#endif
void die(char *format, ...) __attribute__ ((format (printf, 1,2)))__attribute__ ((noreturn)) ;
int dprintf(int level, char *format, ...) __attribute__ ((format (printf, 2, 3)));
void dprintf_file_line_func(int level, char *file_name, int line_number, const char *function_name, char *format, ...) __attribute__ ((format (printf, 5, 6)));
void *salloc(gsize n) __attribute__ ((malloc));
#ifdef OLD
void set_epsilon(double x) __attribute__ ((pure)) ;
double approximately_zero(double x) __attribute__ ((pure)) ;
#endif
void usage(void);
int simple_option_parsing(int argc, char *argv[], char *usage);
int testing_initialize(int *argc, char **argv[], char *usage);
#ifdef NO_G_SLICE
#endif
int initialize(int argc, char *argv[], char *usage);
#ifdef NO_G_SLICE
#endif

/* util/gnuplot.c */

void gnuplotf(char *format, ...);
FILE *gnuplot_open(int width, int height);
void gnuplot(FILE *f, char *format, ...);
void gnuplot_close(FILE *f);

/* util/xv.c */

FILE *xv_open(int width, int height);
void xv_close(FILE *f);
void xv_double_array(int dimension1, int dimension2, double array[dimension1][dimension2], double max, int transpose, int invert);
void write_array_as_image(FILE *f, int dimension1, int dimension2, double array[dimension1][dimension2], double max, int transpose, int invert);
void write_arrays_as_rgb_ppm(char *filename, int dimension1, int dimension2, double red[dimension1][dimension2], double green[dimension1][dimension2], double blue[dimension1][dimension2], int transpose, int invert);
void write_arrays_as_rgb_jpg(char *filename, int dimension1, int dimension2, double red[dimension1][dimension2], double green[dimension1][dimension2], double blue[dimension1][dimension2], int transpose, int invert, char *resize);
void xv_double_array_colour(int dimension1, int dimension2, double red[dimension1][dimension2], double green[dimension1][dimension2], double blue[dimension1][dimension2], int transpose, int invert);
void write_arrays_as_rgb_image(FILE *f, int dimension1, int dimension2, double red[dimension1][dimension2], double green[dimension1][dimension2], double blue[dimension1][dimension2], int transpose, int invert);

/* util/parameter.c */

GKeyFile *param_get_keyfile(char *group, char *key);
GKeyFile *param_set_keyfile(char *group, char *key);
void param_initialize(void);
void param_load_file(const char *directory, const char *filename);
void param_add_config_file(char *pathname, int ignore_missing);
void param_assignment(char *assignment);
int param_get_integer(char *group, char *key);
void param_set_integer(char *group, char *key, int i);
double param_get_double(char *group, char *key);
void param_set_double(char *group, char *key, double d);
char *param_get_string(char *group, char *key);
char *param_get_string_n(char *group, char *key);
void param_set_string(char *group, char *key, char *s);
char *param_sprintf(char *group, char *param, ...);

/* util/sound_io.c */

void sdie(SNDFILE *sf, char *format, ...);
void soundfile_die(soundfile_t *sf, char *format, ...);
soundfile_t *soundfile_open_read(const char *path);
soundfile_t *soundfile_open_write(const char *path, int n_channels, double sampling_rate);
index_t soundfile_read(soundfile_t *sf, sample_t *buffer, index_t n_frames);
index_t soundfile_read_double(soundfile_t *sf, double *buffer, index_t n_frames);
void soundfile_write(soundfile_t *sf, sample_t *buffer, index_t n_frames);
void soundfile_write_double(soundfile_t *sf, double *buffer, index_t n_frames);
void soundfile_close(soundfile_t *sf);
void soundfile_write_entire_double(char *filename, index_t n_frames, int n_channels, double sampling_rate, double *samples);
void soundfile_write_entire(char *filename, index_t n_frames, int n_channels, double sampling_rate, sample_t *samples);
sample_t *soundfile_read_entire(char *filename);
double *soundfile_read_entire_double(char *filename, index_t *n_frames, int *n_channels, double *sampling_rate);
#ifdef OLD
sample_t *read_sound_file(char *filename, index_t *n_frames, int *n_channels, double *sampling_rate);
double *read_sound_file_double(char *filename, index_t *n_frames, int *n_channels, double *sampling_rate);
void write_wav_file(char *filename, index_t n_frames, int n_channels, double sampling_rate, sample_t *samples);
SNDFILE *bsf_open(const char *path, int mode, SF_INFO *sfinfo);
sf_count_t bsf_readf_int(SNDFILE *sndfile, int *ptr, sf_count_t frames);
int bsf_close(SNDFILE *sndfile);
#endif

/* util/approximate_log.c */

double approximate_log(double x);

/* spectral_analysis/power.c */

double create_hann_window(double *window, uint32_t n_window);
double create_hann_window_fp(uint32_t *window, uint32_t n_window);
double create_square_window(double *window, uint32_t n_window);
double create_square_window_fp(uint32_t *window, uint32_t n_window);
void extract_power_phase(sample_t samples[], index_t length, index_t fft_length, int do_windowing, power_t *power, phase_t *phase);
void short_time_power_phase(sample_t samples[], fft_t *f, power_t power[f->n_steps][f->n_bins], phase_t phase[f->n_steps][f->n_bins]);
void free_fft(fft_t *f);
#ifdef USE_FFTW
#endif
#ifdef USE_KISS_FFT
#endif
void fftw_short_time_power_phase(sample_t samples[], fft_t *f, power_t power[f->n_steps][f->n_bins], phase_t phase[f->n_steps][f->n_bins]);
#ifndef USE_FFTW
#else
#endif
void kiss_short_time_power_phase(sample_t samples[], fft_t *f, power_t power[f->n_steps][f->n_bins], phase_t phase[f->n_steps][f->n_bins]);
#ifndef USE_KISS_FFT
#else
#ifdef DOUBLE_POWER_T
#else
#endif
#ifdef DOUBLE_POWER_T
#else
#if POWER_T_BIT_SHIFT < KISS_BIT_SHIFT
#else
#endif
#endif
#ifdef POWER_T_MAX
#else
#endif
#endif
#ifdef OLD
power_t *power_phase(sample_t *restrict buffer, index_t length, index_t fft_length, int do_windowing, phase_t *phase);
#endif

/* spectral_analysis/estimate_sinusoid_parameters.c */

sinusoid_t calculate_refined_sinusoid_parameters(track_t *t, track_point_t *tp);
sinusoid_t estimate_sinusoid_parameters(int bin, int n_bins, power_t power[n_bins], phase_t phase[n_bins], int approximate);
double estimate_phase(int bin, double frequency_delta, phase_t phase[]);
double parabolic_frequency_interpolation(power_t *power, int bin);
double parabolic_amplitude_interpolation(power_t *power, int bin);
#ifdef UNUSED
void reassignment_frequency_correction(sample_t *samples, index_t n_samples, index_t frame_length, index_t fft_length, double sampling_rate, double correct_frequency);
int estimate_sinusoid_parameters(int n_bins, double power[n_bins], double phase[n_bins], double next_phase[n_bins], int overlap, double threshold, int frequency_correction_method, int max_sinusoids, sinusoid_t sinusoids[max_sinusoids]);
#endif

/* spectral_analysis/track_sinusoids.c */

GArray *new_update_sinusoid_tracks(fft_t fft, power_t *power, power_t *previous_power, phase_t phase[restrict fft.n_bins], GArray *active_tracks, int step);
#ifdef OLD		
#endif
GArray *update_sinusoid_tracks(fft_t fft, power_t power_spectrum[restrict fft.n_bins], phase_t phase[restrict fft.n_bins], GArray *active_tracks, int step, int approximate);
#ifdef OLD
GArray *track_sinusoids(fft_t fft, power_t power_spectrum[fft.n_steps][fft.n_bins], phase_t phase[fft.n_steps][fft.n_bins]);
void track_sinusoids1(fft_t fft, power_t power_spectrum[fft.n_bins], phase_t phase[fft.n_bins], GArray *tracks);
#endif
int peaks_to_sinusoids(int n_peaks, int peaks[n_peaks], int n_bins, power_t power[n_bins], phase_t phase[n_bins], int approximate, sinusoid_t sinusoids[n_bins]);
int find_sinusoids(int n_bins, power_t power[n_bins], phase_t phase[n_bins], int bin_offset, int approximate, sinusoid_t sinusoids[n_bins]);

/* spectral_analysis/sinusoid.c */

void add_sinusoid(sample_t *samples, int n_samples, sinusoid_t s);
void add_sinusoid1(sample_t *samples, int n_samples, double amplitude, double phase, double frequency);
void set_sinusoid(sample_t *samples, int n_samples, sinusoid_t s);
void set_sinusoid1(sample_t *samples, int n_samples, double amplitude, double phase, double frequency);
sinusoid_t subtract_sinusoids(sinusoid_t s1, sinusoid_t s2);
sinusoid_t subtract_sinusoids1(sinusoid_t s1, sinusoid_t s2);
sinusoid_t add_sinusoids(sinusoid_t s1, sinusoid_t s2);
sinusoid_t multiply_sinusoids(sinusoid_t s1, sinusoid_t s2);

/* spectral_analysis/peaks.c */

int bins_to_peaks(int length, power_t data[length], int peaks[length]);
int filter_peaks_on_relief(int n_peaks, int peaks[n_peaks], power_t data[], int inside_radius, int outside_radius, double min_relative_relief);
int find_peaks_radius(int length, power_t *restrict data, int *restrict peaks, int radius, power_t min_height);
#ifdef OLD
int find_peaks_radius0(int length, power_t *restrict data, int *restrict peaks);
int find_peaks_radius1(int length, power_t *restrict data, int *restrict peaks);
int find_peaks_radius2(int length, power_t *restrict data, int *restrict peaks);
int find_power_peaks(int n_bins, power_t power[n_bins], int peaks[n_bins]);
int filter_peaks_on_absolute_power(power_t power[], power_t min_power, int n_peaks, int peaks[n_peaks]);
power_t max_peak_power(power_t power[], int n_peaks, int peaks[n_peaks]);
int filter_peaks_on_relative_power(power_t power[], power_t min_relative_power, int n_peaks, int peaks[n_peaks]);
#endif

/* spectral_analysis/track.c */

#ifdef OLD
#endif
void extract_calls_file(char *filename, FILE *index_file, int *call_count);
#ifdef USE_SQLITE
#endif
#ifdef USE_SQLITE
#endif
void score_calls_file(char *filename);
void process_track(track_t *t, fft_t fft, char *source, int channel, index_t step, GArray *past_power, GArray *past_samples, FILE *index_file, int *call_count, sqlite3_stmt *sql_statement);
#ifdef USE_SQLITE
#endif
double score_track(track_t *t);
void track_statistics(track_t *t, double *min_frequency, double *max_frequency, double *sum_frequency, double *min_amplitude, double *max_amplitude, double *sum_amplitude);
#ifdef OLD
void old_track_statistics(track_t *t, double *min_frequency, double *max_frequency, double *sum_frequency, double *min_amplitude, double *max_amplitude, double *sum_amplitude);
#endif
void print_track_attributes(track_t *t, FILE *index_file, FILE *attribute_file);
