#include "bowerbird.h"
#include <sys/types.h>
#include <dirent.h>
#include <complex.h>
#include <fftw3.h>

#include <wavpack/wavpack.h>
#define NUM_STATIONS		3
#define NUM_STATION_PAIRS	3
#define NUM_CHANNELS		4
#define SAMPLING_RATE		16000
#define PI			3.14159265358979323846264338327
#define SPEED_OF_SOUND		340.29

typedef struct
{
	double lat_deg;
	double lat_min;
	double lng_deg;
	double lng_min;
} earthpos_t;
void set_earthpos(earthpos_t *pos,double lat_deg,double lat_min,double lng_deg,double lng_min);

typedef struct point2d_t
{
	double x;
	double y;
} point2d_t;

/* tdoa.c */

#define GCC_PLAIN 0
#define GCC_PHAT  1
#define GCC_ROTH  2
#define GCC_ROTH2 3
#define GCC_SCOT  4
#define GCC_ML    5

/* plotting.c */

#define BLOCKING	0x01
#define NOT_BLOCKING    0x00
/* misc.c */

/* dataman.c */

#define MAX_PATH_LEN	1024
#define MAX_FILENAME_LEN 64
typedef struct
{
	int have_uncompressed;
	int have_compressed;
	//char   uncompressed_filename[MAX_FILENAME_LEN];
	//char   compressed_filename[MAX_FILENAME_LEN];
	char   base_filename[MAX_FILENAME_LEN];
	double time_at_eof;
	int    suffix;
	int    station;

	double *clicktrack;
	index_t nsamples;
} datafile_t;
#define WAVPACK         1
#define UNCOMPRESSED    0

/* other */
#define CHANNEL_NOISE_HEUR	0
#define NUM_CHANNEL_HEUR	1

#define TDOA_TDOA_HEUR		0
#define TDOA_BADNESS_HEUR	1
#define NUM_TDOA_HEUR		2

#define LOCATION_CONSISTENCY_HEUR 0
#define LOCATION_LATDEG_HEUR 1
#define LOCATION_LATMIN_HEUR 2
#define LOCATION_LNGDEG_HEUR 3
#define LOCATION_LNGMIN_HEUR 4
#define NUM_LOCATION_HEUR	5
typedef struct channel_s
{
	double heuristics[NUM_CHANNEL_HEUR];
	int station, channel;
	double *waveform;
	index_t nsamples;
} channel_t;
typedef struct tdoa_result_s
{
	double heuristics[NUM_TDOA_HEUR];
	double tdoa;
	channel_t *channel1;
	channel_t *channel2;
} tdoa_result_t;

typedef struct estimate_s
{
	double heuristics[NUM_LOCATION_HEUR];
	earthpos_t location;
	double uncertainty;
	tdoa_result_t *tdoa_01;
	tdoa_result_t *tdoa_12;
	tdoa_result_t *tdoa_20;
	double start_time;
	double length;
} estimate_t;



void locate_point(double TDOA_01_in_seconds,double TDOA_12_in_seconds,double TDOA_20_in_seconds,earthpos_t station_earthpos[],earthpos_t *location,double heuristics[]);


void write_waveform(char *file, double *waveform, index_t nsamples, int compression);

/* select.c */
#define VALUE_ABOVE		0
#define VALUE_BELOW		1
#define POSITION_FROM_TOP	2
#define POSITION_FROM_MIDDLE	3
#define POSITION_FROM_BOTTOM	4
#define PERCENT_FROM_TOP	5
#define PERCENT_FROM_MIDDLE	6
#define PERCENT_FROM_BOTTOM	7

#define	MAX_FILES	4096
#include "localization-prototypes.h"
