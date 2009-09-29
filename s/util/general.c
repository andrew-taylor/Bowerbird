#include "i.h"

/*
 * verbosity == 0   no debug output
 *            1-10  debug output potentially of interest to users
 *           10-20  debug output only of interest to develops but not prohibitively verbose
 *           20+    prohibitively verbose debug output
 */
 
int		verbosity = 0;
FILE	*debug_stream;
char	*myname;
char	*version;

void
set_myname(char *argv[]) {
	myname = strrchr(argv[0], '/');
	if (myname == NULL)
		myname = argv[0];
	else
		myname++;
}
//__attribute__ ((format (printf, 1,2)))
//__attribute__ ((noreturn)) 
void
die(char *format, ...) {
	va_list ap;
	if (myname)
		fprintf(stderr, "%s: ", myname);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	if (errno == 0)
		fprintf(stderr, "\n");
	else
		perror(" ");
	if (verbosity > 9) {
		fprintf(stderr, "Dumping core for debugging\n");
		abort();
	}
	exit(1);
}


//__attribute__ ((format (printf, 2, 3)))
int
dprintf(int level, char *format, ...) {
	va_list ap;
	if (level > verbosity)
		return 0;
	if (!debug_stream)
		debug_stream = stderr;
	va_start(ap, format);
	return vfprintf(debug_stream, format, ap);
}

//__attribute__ ((format (printf, 5, 6)))
void
dprintf_file_line_func(int level, char *file_name, int line_number, const char *function_name, char *format, ...) {
	va_list ap;
	if (level > verbosity)
		return;
	if (!debug_stream)
		debug_stream = stderr;
	char *f = strrchr(file_name, '/');
	if (f)
		file_name = f+1;
	fprintf(debug_stream, "%s:%d %s:", file_name, line_number, function_name);
	va_start(ap, format);
	vfprintf(debug_stream, format, ap);
}

// deprecated. now using #define in bowerbird.h
//static double  epsilon = 1000*DBL_EPSILON; // sqrt(DBL_EPSILON) would be better
//
//void
//set_epsilon(double x) {
//	epsilon = x;
//}
//
////__attribute__ ((pure)) 
//double
//approximately_zero(double x) {
//	return -epsilon < x && x < epsilon;
//}

// not sure where this might be used?
//void
//usage(void) {
//	fprintf(stderr, "Usage: %s  <wav files>\n", myname);
//    exit(1);
//}

static char *short_options = "o:C:V:v:";

static struct option long_options[] = {
	{"option", 1, 0, 'o'},
	{"config-file", 1, 0, 'C'},
	{"verbosity", 1, 0, 'v'},
	{"version", 1, 0, 'V'},
	{0, 0, 0, 0}
};

int
simple_option_parsing(int argc, char *argv[], const char *config_group, const char *version, const char *usage) {
	set_myname(argv);
	param_initialize();
	errno = 0;  // handy place to clear any previous errno
	while (1) {
		int option_index;
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (c == -1)
			break;
		opterr = 0;
		switch (c) {
		case 'o':
			param_assignment(optarg, config_group);
 			break;
		case 'C':
			param_add_config_file(optarg, 0);
			break;
		case 'v':
			verbosity = atoi(optarg);
  			break;
		case 'V':
			printf("%s v%s\n",myname, version);
  			exit(0);
		case '?':
			fprintf(stderr, "Usage: %s [-v<verbosity>] [-o<parameter>=<value>] [-C <config-file>] %s\n", myname, usage);
			exit(1);
 		}
 	}
 	return optind;
}

int
testing_initialize(int *argc, char **argv[], const char *usage) {
	verbosity = 1;
#ifdef NO_G_SLICE
//	dp(11, "g_setenv(\"G_SLICE\", \"always-malloc\", 1)\n");
	g_setenv("G_SLICE", "always-malloc", 1);
#endif
	g_test_init(argc, argv, NULL); 
	return simple_option_parsing(*argc, *argv, NULL, "-1", "");
}

int
initialize(int argc, char *argv[], const char *config_group, const char *version, const char *usage) {
#ifdef NO_G_SLICE
	g_setenv("G_SLICE", "always-malloc", 1);
#endif
	return simple_option_parsing(argc, argv, config_group, version, usage);
}


double
bound(double value, double min, double max)
{
	double diff = max - min;
	while (value < min)
		value += diff;
	while (value >= max)
		value -= diff;
	return value;
}


double
clamp(double value, double min, double max)
{
	if (value < min)
		return min;
	if (value > max)
		return max;
	return value;
}
