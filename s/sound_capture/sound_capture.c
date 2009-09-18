#include "i.h"

#define CONFIG_GROUP "sound_capture"

#define WAV_HEADER_SIZE 44

#define VERSION "0.1.1"


int main(int argc, char *argv[]) {
	debug_stream = stderr;
	initialize(argc, argv, VERSION, "");
	verbosity = 30;//1;
	run();
	return 0;
}


void run(void) {
	struct timeval  tv ;
	const int n_channels = param_get_integer(CONFIG_GROUP, "alsa_n_channels");
	const int buffer_frames = param_get_integer(CONFIG_GROUP, "sound_buffer_frames");
	const int sampling_rate = param_get_integer(CONFIG_GROUP, "alsa_sampling_rate");
	const char *file_dir = param_get_string(CONFIG_GROUP, "sound_file_dir");
	const char *file_name = param_get_string(CONFIG_GROUP, "sound_file_name");
	const char *details_name = param_get_string(CONFIG_GROUP, "sound_details_name");
	const int n_files = param_get_integer(CONFIG_GROUP, "sound_n_files");

	int16_t *buffer = salloc(n_channels*buffer_frames*sizeof *buffer); 
    struct sigaction reapchildren = {{0}};
	reapchildren.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &reapchildren, 0);

	do_alsa_init(param_get_string(CONFIG_GROUP, "alsa_pcm_name"),
			param_get_integer(CONFIG_GROUP, "alsa_sampling_rate"),
    		param_get_integer(CONFIG_GROUP, "alsa_n_periods"),
			param_get_integer(CONFIG_GROUP, "alsa_periods_size"),
			param_get_integer(CONFIG_GROUP, "alsa_n_channels"),
			param_get_integer(CONFIG_GROUP, "alsa_buffer_size"));

//	int beep_enabled = param_get_boolean(CONFIG_GROUP, "beep");
	unsigned int file_count = 0;
	dp(30, "starting loop\n");
	for (int i = 0;;++i) {
		dp(30, "loop %d\n", i);
		const int length = alsa_readi(buffer, buffer_frames);
		gettimeofday(&tv, NULL);
		time_t t = time(NULL);
		if (length <= 0)
			continue;
//		if (beep_enabled) {
//			beep_enabled = FALSE;
//			int active_high = param_get_boolean(CONFIG_GROUP, "active_high");
//			beep(1, 1000, 1, active_high);
//			msleep(300);
//			beep(1, 2000, 1, active_high);
//			msleep(300);
//			beep(1, 3000, 1, active_high);
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
	char wav_header[WAV_HEADER_SIZE];
	set_16bit_WAV_header(wav_header, n_frames, n_channels, sampling_rate);
	dp(30, "pathname=%s details_pathname=%s\n", pathname, details_pathname);
	unlink(details_pathname);
	unlink(pathname);
	switch (param_get_integer(CONFIG_GROUP, "sound_compression_type")) {
		case 0:
			write_wav_data(wav_header, buffer, n_channels, n_frames, pathname);
			break;
		case 1:
			write_wavpack_data(wav_header, buffer, n_channels, n_frames, sampling_rate, pathname);
			break;
		case 2:
			fork_write_cmd(wav_header, buffer, n_channels, n_frames, pathname);
			break;
		default:
			die("unknown compression type '%d' requested", param_get_integer(CONFIG_GROUP, "sound_compression_type"));
	}
	FILE *fp = fopen(details_pathname, "w");
	assert(fp);
	fprintf(fp, "%lu.%06lu@%02d\n", (unsigned long)seconds, (unsigned long)microseconds, 10*calculate_monitoring_priority(seconds));
	fclose(fp);
}


void
write_wav_data(char *wav_header, int16_t *buffer, int n_channels, int n_frames, char *pathname)
{
	int fd = open(pathname, O_WRONLY|O_CREAT, 0666);
	assert(fd >= 0);
	if (write(fd, wav_header, WAV_HEADER_SIZE) != WAV_HEADER_SIZE)
		die("write header failed");
	size_t write_size = n_channels*n_frames*sizeof *buffer;
	if (write(fd, buffer, write_size) != write_size)
		die("write data failed");
	close(fd);
}


void
write_wavpack_data(char *wav_header, int16_t *buffer, int n_channels, int n_frames, int sampling_rate, char *pathname)
{
	soundfile_t *s = soundfile_open_write(pathname, n_channels, sampling_rate);
	soundfile_write_header(s, wav_header, WAV_HEADER_SIZE);
	soundfile_write(s, buffer, n_frames);
	soundfile_close(s);
}


void
fork_write_cmd(char *wav_header, int16_t *buffer, int n_channels, int n_frames, char *pathname)
{
	char *compressor = param_get_string(CONFIG_GROUP, "sound_compressor_shellcmd");
	if (!compressor && strlen(compressor) == 0) {
		dp(1, "external compressor requested, but not configured");
		return;
	}

	switch (fork()) {
		case 0:
			// child continues execution
			break;
		case -1:
			dp(1,"fork failed\n");
			return;
		default:
			// parent returns immediately
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
	char command[PATH_MAX+256];
	snprintf(command, sizeof command, compressor, pathname);
	dp(1, "command='%s'\n", command);
	FILE *f = popen(command, "w");
	assert(f);
	if (fwrite(wav_header, WAV_HEADER_SIZE, 1, f) != 1)
		die("fwrite header failed");
	if (fwrite(buffer, sizeof *buffer, n_channels*n_frames, f) != n_channels*n_frames) 
		die("fwrite data failed");
	fclose(f);
	dp(1,"child complete\n");
	exit(0);
}


void
set_16bit_WAV_header(char *h, int frames, int channels, int frequency) {
	dp(30, "creating wav header, frames %d, channels %d, frequency %d\n", frames, channels, frequency);
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
