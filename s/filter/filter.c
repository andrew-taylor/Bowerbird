#include "comb_filter.h"

static void usage(void) {
	fprintf(stderr, "Usage %s [-V] [-v verbosity] <commands> \n", myname);
}

char *version = "0.1";

static char *short_options = "o:v:V";
static struct option long_options[] = {
	{"option", 1, 0, 'o'},
	{"verbosity", 1, 0, 'v'},
	{"version", 0, 0, 'V'},
	{0, 0, 0, 0}
};

void
initialize_options(void) {
	epsilon = sqrt(DBL_MIN);
	set_option("input_file", "");
	set_option_int("sound_buffer_size", 64000);
	set_option_int("sound_buffer_frames", 1*60*16000);
	
	set_option("sound_file_dir", "ramfs/share");
	set_option("sound_file_name", "%s/%d.sw");
	set_option("sound_details_name", "%s/%d.details");
	set_option("sound_details_name", "%s/%d.details");
	set_option_int("sound_n_files", 3);
	set_option_int("sound_n_skip", -1);
	set_option_int("sound_mono_buffer_size", 65536);
	set_option_boolean("sound_to_stdout", 0);
	set_option_boolean("sound_multiplier", 0);
	
	set_option("alsa_pcm_name", "hw:0,0");
	set_option_int("alsa_n_channels", 2);
	set_option_int("alsa_sampling_rate", 16000);
	set_option_int("alsa_periods_size", 16000);
	set_option_int("alsa_n_periods", 2);
	set_option_int("alsa_buffer_size", 16384);
	
	set_option_boolean("active_high", 0);
	set_option_boolean("beep", 0);
	
	set_option_int("watchdog_sleep_seconds", 5);
	set_option_int("watchdog_max_start_seconds", 1200);
	set_option("watchdog_path1", "ramfs/share/");
	set_option_int("watchdog_path1_seconds", 7200);
	set_option("watchdog_path2", "ramfs/network_up");
	set_option_int("watchdog_path2_seconds", 24*3600);
	
	set_option_double("comb_filter_frequency", 1000);
	set_option_double("comb_filter_feedback_weight", 0.04);
	
}
void
postprocess_options(void) {
}

int main(int argc, char *argv[]) {
	verbosity = 1;
	debug_stream = stderr;
	set_myname(argv);
	optind = 0;
	initialize_options();
	while (1) {
		int option_index;
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (c == -1)
			break;
		opterr = 0;
		switch (c) {
		case 'o':
			parse_option_assignment(optarg);
 			break;
		case 'v':
			verbosity = atoi(optarg);
  			break;
		case 'V':
			printf("%s v%s\n",myname, version);
  			exit(0);
		case '?':
			usage();
 		}
	}
	postprocess_options();
	comb("/dev/stdin", "/dev/stdout");
	return 0;
}
