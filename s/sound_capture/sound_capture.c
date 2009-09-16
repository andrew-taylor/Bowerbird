#include "i.h"

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
//	char *input_file = get_option("input_file");
//	if (input_file && strlen(input_file)) {
//		test_wavpack(input_file);
//		exit(0);
//	}
	run();
	return 0;
}

void
initialize_options(void) {
	set_option("input_file", "");
	set_option_int("sound_buffer_size", 64000);
	set_option_int("sound_buffer_frames", 60*16000);
	
	set_option("sound_file_dir", "/tmp/s/");
	set_option("sound_details_name", "%s/%d.details");
	set_option("sound_file_name", "%s/%d.wv");
	set_option("sound_compressor", "/usr/bin/wavpack -q -f - >%s");
	
	set_option("alsa_pcm_name", "quad");
	set_option_int("alsa_n_channels", 4);
	set_option_int("alsa_sampling_rate", 16000);
	set_option_int("alsa_periods_size", 16000);
	set_option_int("alsa_n_periods", 2);
	set_option_int("alsa_buffer_size", 16384);
	
	set_option_boolean("beep", 0);
	set_option_boolean("active_high", 0);
	
	set_option_int("sound_n_files", 4);
}


void run(void) {
	struct timeval  tv ;
	const int n_channels = get_option_int("alsa_n_channels");
	const int buffer_frames = get_option_int("sound_buffer_frames");
	const int sampling_rate = get_option_int("alsa_sampling_rate");
	const char *file_dir = get_option("sound_file_dir");
	const char *file_name = get_option("sound_file_name");
	const char *details_name = get_option("sound_details_name");
	const int n_files = get_option_int("sound_n_files");

	int16_t *buffer = salloc(n_channels*buffer_frames*sizeof *buffer); 
    struct sigaction reapchildren = {{0}};
	reapchildren.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &reapchildren, 0);
	do_alsa_init();
	
//	int beep_done = 0;
	unsigned int file_count = 0;
	dp(30, "starting loop\n");
	for (int i = 0;;++i) {
		dp(30, "loop %d\n", i);
		const int length = alsa_readi(buffer, buffer_frames);
		gettimeofday(&tv, NULL);
		time_t t = time(NULL);
		if (length <= 0)
			continue;
//		if (!beep_done) {
//			beep_done = 1;
//			beep(1, 1000, 1);
//			msleep(300);
//			beep(1, 2000, 1);
//			msleep(300);
//			beep(1, 3000, 1);
//		}
		if (file_count >= n_files)
			file_count = 0;
		char details_pathname[PATH_MAX];
		char pathname[PATH_MAX];
		snprintf(details_pathname, sizeof details_pathname, details_name, file_dir, file_count);
		snprintf(pathname, sizeof pathname, file_name, file_dir, file_count, t);
		file_count++;
		write_data(buffer, n_channels, length, sampling_rate, pathname, details_pathname, tv.tv_sec, (uint32_t)tv.tv_usec);
	}
}

// popen/pclose is not c99
extern FILE *popen(const char *command, const char *type); 
extern int pclose(FILE *stream);

void
write_data(int16_t *buffer, int n_channels, int n_frames, int sampling_rate, char *pathname, char *details_pathname, time_t seconds, uint32_t microseconds) {
	switch (fork()) {
	case 0:
		break;
	case -1:
		dp(1,"fork failed\n");
		return;
	default:
		return;
	}
	// nice returns -1 on failure, but also when nice value is -1, 
	// so we check errno as well.
	errno = 0;
	if (nice(10) == -1 && errno != 0)
		die("nice failed");
	dp(1,"child starts\n");
	for (int i=3; i < 30;i++)
		close(i);
	char wav_header[44];
	set_16bit_WAV_header(wav_header, n_frames, n_channels, sampling_rate);
	dp(30, "pathname=%s details_pathname=%s\n", pathname, details_pathname);
	unlink(details_pathname);
	unlink(pathname);
	char *compressor = get_option("sound_compressor");
	if (compressor && strlen(compressor)) {
		char command[PATH_MAX+256];
		snprintf(command, sizeof command, compressor, pathname);
		dp(1, "command='%s'\n", command);
		FILE *f = popen(command, "w");
		assert(f);
		if (fwrite(wav_header, sizeof wav_header, 1, f) != 1)
			die("fwrite header failed");
		if (fwrite(buffer, sizeof *buffer, n_channels*n_frames, f) != n_channels*n_frames) 
			die("fwrite data failed");
		fclose(f);
	} else {
		int fd = open(pathname, O_WRONLY|O_CREAT, 0666);
		assert(fd >= 0);
		size_t write_size = sizeof wav_header;
		if (write(fd, wav_header, write_size) != write_size)
			die("write header failed");
		write_size = n_channels*n_frames*sizeof *buffer;
		if (write(fd, buffer, write_size) != write_size)
			die("write data failed");
		close(fd);
	}	
	FILE *fp = fopen(details_pathname, "w");
	assert(fp);
	fprintf(fp, "%lu.%06lu@%02d\n", (unsigned long)seconds, (unsigned long)microseconds, 10*calculate_monitoring_priority(seconds));
	fclose(fp);
	dp(1,"child complete\n");
	exit(0);
}


void
set_16bit_WAV_header(char *h, int frames, int channels, int frequency) {
    int bytesPerSample = channels*2;
    int bitsPerSample = 16;

    strcpy(h, "RIFF    WAVEfmt ");
    intcpy(h + 4, 36 + frames*bytesPerSample);
    intcpy(h + 16, 16);
    shortcpy(h + 20, 1);
    shortcpy(h + 22, channels);
    intcpy(h + 24, frequency);
    intcpy(h + 28, frequency*bytesPerSample);
    shortcpy(h + 32, bytesPerSample);
    shortcpy(h + 34, bitsPerSample);
    strcpy(h + 36, "data");
    intcpy(h + 40, frames*bytesPerSample);
}

void
intcpy(char *b, int i) {
    b[3] = (i >> 24);
    b[2] = ((i >> 16) & 0xff);
    b[1] = ((i >> 8) & 0xff);
    b[0] = (i & 0xff);
}

void
shortcpy(char *b, int i) {
    b[1] = ((i >> 8) & 0xff);
    b[0] = (i & 0xff);
}
