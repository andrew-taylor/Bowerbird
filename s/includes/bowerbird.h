#ifndef BOWERBIRD_H
#define BOWERBIRD_H
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include <math.h>
#include <sndfile.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_statistics_double.h>
#include "gsl/gsl_cdf.h"
#include <complex.h>

#ifdef USE_SQLITE
#include <sqlite3.h>
#else
typedef sqlite3_stmt void; // kludge
#endif

// global types


typedef uint32_t index_t;  // sufficient to hold length of longest sound handled
//typedef double sample_t;   // for sound samples SF_READF_SAMPLE_T,SF_WRITEF_SAMPLE_T must match
typedef short sample_t;   // for sound samples
typedef double phase_t;    // for signal phase

#define SAMPLE_T_BIT_SHIFT 15
#define SAMPLE_T_DIVISOR (1 << SAMPLE_T_BIT_SHIFT)
#define sample_t_to_double(x) ((x)/(double)SAMPLE_T_DIVISOR)
#define double_to_sample_t(x) ((sample_t)(round(SAMPLE_T_DIVISOR*(x))))
#define phase_t_to_double(x) (x)
#define double_to_phase_t(x) (x)

// power.c assumes KISS_BIT_SHIFT <= 32 

#ifdef FIXED_POINT
#define KISS_BIT_SHIFT (FIXED_POINT-1)
#define KISS_FIXED_POINT_SCALE (((uint32_t)1) << KISS_BIT_SHIFT)
#define double_to_kiss(d) ((d)*KISS_FIXED_POINT_SCALE)
#define kiss_to_double(f) ((f)/(double)KISS_FIXED_POINT_SCALE)
#define sample_t_to_kiss(d) ((d)*(KISS_FIXED_POINT_SCALE/SAMPLE_T_DIVISOR))
#define kiss_to_sample_t(f) ((f)/(KISS_FIXED_POINT_SCALE/SAMPLE_T_DIVISOR))
#define kiss_to_power_t(d) ((d)*(POWER_T_SCALE/KISS_FIXED_POINT_SCALE))
#else
#define kiss_to_double(d) ((double)(d))
#endif

#ifdef POWER_T_DOUBLE
typedef double power_t;      // for signal power
#define double_to_power_t(x) (x)
#define power_t_to_double(x) (x)
#else
#ifdef POWER_T_32
typedef uint32_t power_t;    // for signal power
#define POWER_T_BIT_SHIFT 28
#define POWER_T_SCALE (((uint32_t)1) << POWER_T_BIT_SHIFT)
#define POWER_T_MAX 4294967295
#else
typedef uint64_t power_t;    // for signal power
#define POWER_T_BIT_SHIFT 50
#define POWER_T_SCALE (((uint64_t)1) << POWER_T_BIT_SHIFT)
#endif

#define double_to_power_t(d) ((d)*POWER_T_SCALE)
#define power_t_to_double(f) ((f)/(double)POWER_T_SCALE)
#endif

//#define SF_READF_SAMPLE_T sf_readf_double
//#define SF_WRITEF_SAMPLE_T sf_writef_double
#define SF_READF_SAMPLE_T sf_readf_short
#define SF_WRITEF_SAMPLE_T sf_writef_short

// definitions from util

#define assert(expr) ((void)((expr) ? 0 : die("%s:%d: assertion %s failed in function %s",  __FILE__ , __LINE__, #expr, __func__)))
//#define dp(level, ...) (dprintf(level, "%s:%d: %s ", __FILE__ , __LINE__, __func__),dprintf(level,  __VA_ARGS__))
#define dp(level, ...) (level <= verbosity ? dprintf(level, "%s:%d: %s ", __FILE__ , __LINE__, __func__),dprintf(level,  __VA_ARGS__) : 0)
#define approximately_zero(x) (ABS(x) < sqrt(DBL_EPSILON))

typedef struct sinusoid_t {
	double amplitude;
	double phase;
	double frequency;
} sinusoid_t;

typedef struct {
	enum {sft_libsndfile, sft_wavpack} t;
	enum {sft_read, sft_write} m;
	uint32_t channels;
	uint32_t samplerate;
	uint32_t frames;
	uint32_t bits_per_sample; // set only for sft_wavpack
	void *p;
	// next 3 fields for writing wvpack 
    uint32_t bytes_written, first_block_size;
    FILE *file;
    int error;
} soundfile_t;

// definitions from spectral analysis

typedef enum fft_window_t {
	fw_hann,
	fw_none,
} fft_window_t;
	
typedef struct fft_t {
	uint32_t        fft_size;
	uint32_t		window_size;    // must be <= fft_size
	uint32_t		step_size;      // must be <= window_size
	fft_window_t	window_type;
	uint32_t		n_bins;         // for convenience - must be (fft_size+1)/2
	index_t			n_steps;        // optional, used for stft
	double			sampling_rate;  // optional, not used in fft, but convenient to include here
	void			*window;        // double for fftw, uint32_t for kiss_fft
	double 			window_correction;
	void 			*in;
	void		 	*out;
	void			*state;
} fft_t;

typedef struct track_t {
	index_t start;      // starting time of track
	GArray  *points;    // array of track_point_t
	int     completed;  // boolean indicating whether finish of track has been see 
	int     gap;        // time steps since peak seen
	power_t last_peak_power;
	fft_t	fft;
} track_t;

typedef struct track_point_t {
	int bin;
	power_t power[3];
	phase_t phase[3];
//	sinusoid_t s;
} track_point_t;

typedef enum distribution_type {
	dt_gaussian,
	dt_constant,
	dt_uniform,
	dt_rayleigh,
} distribution_type_t;

typedef struct distribution {
	double		weight;
	distribution_type_t	type;
	double		a;  // mean for normal, minimum for rayleigh, min for uniform
	double		b;  // standard deviation for normal, scale for rayleigh, max for uniform
} distribution_t;

typedef struct mixture_t {
	int				n_components;
	distribution_t	*components; // n_components
} mixture_t;

typedef struct energy_trigger {
	uint64_t	n_frames_seen;
	uint32_t	interval_length;
	double		threshold_average_energy;
	double		threshold_total_energy;
	double		*energy_buffer;
	uint32_t	energy_buffer_index;
	double		total_energy;
	double		maximum_total_energy;
	uint64_t	triggered;
} energy_trigger_t;


// not part of ISO
FILE *popen(const char *command, const char *type);
int pclose(FILE *stream);

// yuk
#define MAX_STATES	16
#define MAX_SYMBOLS	16

typedef struct hmm {
	int initial_state;
	int n_states;
	int n_symbols;
	double emission_probability[MAX_STATES][MAX_SYMBOLS];
	double transition_probability[MAX_STATES][MAX_STATES];
} hmm;


// multi-variate continuous hmm
typedef struct cm_hmm_t {
	int initial_state;
	int n_states;
	int dimension;
	mixture_t *mixture;        // [dimension][n_states]
	double *transition_probability; // [n_states][n_states];
} cm_hmm_t;


#ifndef __GNUC__
#define __attribute__()

#endif
#include "prototypes.h"
#endif
