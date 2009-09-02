
/* localization/select.c */

void g_ptr_array_remove_range_and_free(GPtrArray *array,int start, int length);
int select_percentage(GPtrArray *array, int heur_id, int query_type, double percent);
int select_position(GPtrArray *array, int heur_id, int query_type, double threshold);
int select_value(GPtrArray *array, int heur_id, int query_type, double threshold);
double value_percentage(GPtrArray *array, int heur_id, int query_type, double threshold);
double value_position(GPtrArray *array, int heur_id, int query_type, double threshold);
extern int static_sort_by_heuristic_heur_id;
int cmp_heuristic(const void *p1, const void *p2);
void sort_by_heuristic(GPtrArray *array, int heur_id);
double value(GPtrArray *array, int heur_id, int query_type, double argument);
void select_single(GPtrArray *array, int heur_id, int query_type, double argument);
void select_main(GPtrArray *array[NUM_STATIONS], int heur_id, int query_type, double argument);
int drop_single(GPtrArray *array, int heur_id, int query_type, double argument);
void drop(GPtrArray *array[NUM_STATIONS], int heur_id, int query_type, double argument);

/* localization/misc.c */

extern char *result_file;
double minwithindex(double *array, int N, index_t *index);
double maxwithindex(double *array, int N, index_t *index);
double max(double *array, int N);
double min(double *array, int N);
double median(double *array, int N);
int cmp_double(const void *p1, const void *p2);
int num_digits(int x);
int zero_leastsig_digits(int x,int numtodrop);
double parse_time(char *timestr);
void fprint_date(FILE *fp,double epoch_timestamp);
void fprint_location(FILE *fp, earthpos_t *location);
int geochar_sgn(char c);
void set_earthpos(earthpos_t *pos,double lat_deg,double lat_min,double lng_deg,double lng_min);
double deg2rad(double degree,double minutes);
double deg2full(double deg,double min);
extern FILE *fp_result;
extern int result_count;
void write_result(double time,double length,earthpos_t *location,double uncertainty);
double sgn(double x) __attribute__ ((const));
double square_d(double x) __attribute__ ((const)) ;

/* localization/dataman.c */

extern char *base_dir;
extern char *date_dir;
extern char *station_dir[NUM_STATIONS];
extern double click_threshold;
extern int compress_clicktracks; 
extern struct tm *base_date; 
extern time_t     base_epoch;
extern datafile_t files[NUM_STATIONS][MAX_FILES];
extern int num_files[NUM_STATIONS];
void dataman_cleanup(void);
int file_compare(const void *p1, const void *p2);
int file_consider(const void *p1, const void *p2);
int binary_search(void *array, size_t nmemb, size_t size, void *key, int(*consider)(const void*, const void *));
void dataman_scan(void);
int sprint_filepath(char *output,datafile_t *file);
double *read_raw(char *filename,index_t *nsamples,int *nchannels,double *sampling_rate,int compression);
void write_waveform(char *file, double *waveform, index_t nsamples, int compression);
index_t read_all_channels(double *output[], char *filename, index_t start, index_t len, int compressed);
index_t read_waveform(double **output, char *filename, int channel, index_t start, index_t len, int compressed);
index_t read_whole_waveform(double **output, char *filename, int channel, int compression);
index_t find_click(double *waveform, index_t position, index_t end, double threshold);
void generate_clicktrack(datafile_t *file);
void read_clicktrack(datafile_t *file);
index_t find_nearest_second(int station, int fileidx, index_t position, int *sgn, int *side);
index_t find_position(int station, double time_in_seconds, int *fileidx_);
index_t determine_nsamples(char *filename, int compression);
void grab_station_waveforms(double *waveforms[], int station, double start_in_seconds, double length_in_seconds);

/* localization/sigproc.c */

double*filter(double *in, index_t nsamples, double lowfreq, double highfreq);
double*bandpass_filter(double *in, index_t nsamples, double centre, double width);
double*highpass_filter(double *in, index_t nsamples, double lowfreq);
void plotpowerspectrum(double *inorig, index_t nsamples,int fft_size);
double *lowpass_old(double *in_,index_t nsamples, double lowfreq, int fft_size);

/* localization/geometry.c */

double degree_of_latitude(double lat_rad);
double degree_of_longitude(double lat_rad);
double normsquared2(point2d_t point);
double norm2(point2d_t point);
double dist2(point2d_t *p1, point2d_t *p2);
double distance_from_latlng_radians(double lat1,double lng1, double lat2, double lng2);
double earth_distance(earthpos_t *pos1, earthpos_t *pos2);
void earth_to_relative_2d_position(point2d_t *relative_2d_position, earthpos_t *thispoint, earthpos_t *refpoint);
void relative_2d_to_earth_position(earthpos_t *result, point2d_t *relative, earthpos_t *ref);
void locate_point(double TDOA_01_in_seconds,double TDOA_12_in_seconds,double TDOA_20_in_seconds,earthpos_t station_earthpos[],earthpos_t *location,double heuristics[]);
void rotate2(point2d_t *p,double slope,point2d_t *origin);
void flip(point2d_t *p);
double fitnessfunc(point2d_t *p,double del01,double del12,double del20,point2d_t *stations);
void locate_point_in_plane(double TDOA_01,double TDOA_12,double TDOA_20,point2d_t stations[],point2d_t *location,double *heuristics);
#if 0
#endif
#if 0
void pinpoint_via_tdoa(double TDOA_01,double TDOA_12,double TDOA_20,point2d_t stations[],point2d_t *bestpoint);
void rotate90(point2d_t *p);
void rotate270(point2d_t *p);
void rotate2_(point2d_t *p,double slope);
void searchforit(point2d_t *startpoint, point2d_t *endpoint, double granularity, point2d_t *bestpoint, double del01, double del12, double del20, point2d_t *stations);
void rotate(point2d_t *point, double theta);
void pivot_rotate2(point2d_t *point, double theta, point2d_t *pivot);
point2d_t *construct_hyperbola(point2d_t *focus1, point2d_t *focus2, double constant_difference, int *num_);
#endif

/* localization/tdoa.c */

fftw_complex *crosspower_spectrum(double *waveform1, double *waveform2, index_t nsamples);
double time_difference_of_arrival(double *waveform1,double *waveform2,index_t nsamples,int method,double *heuristic);

/* localization/plotting.c */

void plotreal(double *wav, index_t len);
void plotrealx(double *xs, double *wav, index_t len);
void plot2real(double *wav1, index_t len1, double *wav2, index_t len2);
void plot3real(double *wav1, index_t len1, double *wav2, index_t len2, double *wav3, index_t len3);
void plot4real(double *wav1, index_t len1, double *wav2, index_t len2, double *wav3, index_t len3, double *wav4, index_t len4);
void plotcomplex(fftw_complex *out, index_t nsamples);
void plotcomplexx(double *xs, fftw_complex *out, index_t nsamples);
void pyplotpointdata(point2d_t *points,int num_points, int blocking);
void pyplotrealx(double *xs, double *ys, index_t len);

/* localization/kml.c */

extern char *kml_file;
extern FILE *fp_kml;
void write_kml(char *name, earthpos_t *A);
extern const char *xml_header;
extern const char *kml_namespace;
extern const int spaces_per_level;
void fprintf_tagline(FILE *fp,int level,char *tag,char *format, ...);
void fputs_tag(int level,char *str,FILE *fp);
void write_kml_coordinates(FILE *fp,int level,earthpos_t *point);
extern char *stationdescriptions[NUM_STATIONS];
void write_kml_point(FILE *fp,int level,earthpos_t *point,char *name,char *style,char *desc);
void write_kml_stations(FILE *fp,int level,earthpos_t *stations);
void write_kml_known(FILE *fp,int level,GHashTable *known);
void write_kml_estimated(FILE *fp,int level,GPtrArray *estimates);
void fprintf_tagsingle(FILE *fp,int level,char *format,...);
void write_kml_basicstyle(FILE *fp_kml,int level,char *stylename,char *stylelocation);
extern char *bigredstar_location;
extern char *red0_location;
extern char *red1_location;
extern char *red2_location;
extern char *red3_location;
extern char *red4_location;
extern char *red5_location;
extern char *red6_location;
extern char *red7_location;
extern char *red8_location;
extern char *red9_location;
extern char *red10_location;
void write_kml_file(earthpos_t *stations, GHashTable *known_positions, GPtrArray *estimates);
extern int level;

/* localization/localize.c */

extern double BREATHING_SPACE;
extern int graphing;
extern earthpos_t *current;
void load_station_positions(earthpos_t stations[]);
estimate_t *analyze_region(double start_in_seconds, double length_in_seconds);
#if 0
#endif
void load_config(void);
void analyze_channel_noise(channel_t *chan);
void process_file(char *filename);
#if 0
void test_analyze_channel(channel_t *chan);
int select(GPtrArray *array[NUM_STATIONS], int heur_id, int query_type, int threshold);
double analyze_regionCURRENT(double start_in_seconds, double length_in_seconds, earthpos_t *bestpoint_on_earth);
#endif
