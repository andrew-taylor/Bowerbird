#include "i.h"

#define SOUND_CAPTURE_GROUP "sound_capture"
#define VERSION "0.1.1"
#define FILE_DIR_FORMAT "%d_%02d_%02d" // args are year, month, day
#define FILE_NAME_FORMAT "%s/%s/%02d_%02d_%02d_%06d.%s" // args are data_dir, file_dir(date), hours, minutes, seconds, microseconds, file extension

#define PARAMETER_PARSING_VERBOSITY 20 // verbosity for parsing cmdline & file parameters
#define PROGRAM_VERBOSITY 30

#define WAV_HEADER_SIZE 44


int main(int argc, char *argv[]) 
{
	debug_stream = stderr;
	initialize(argc, argv, SOUND_CAPTURE_GROUP, VERSION, "");
	
	run();
	return 0;
}


void run(void) 
{
	struct timeval  tv ;
	const int duration = param_get_integer_with_default(SOUND_CAPTURE_GROUP, "recording_duration", 0);
	const int n_channels = param_get_integer(SOUND_CAPTURE_GROUP, "alsa_n_channels");
	const int buffer_frames = param_get_integer(SOUND_CAPTURE_GROUP, "sound_buffer_frames");
	const int sampling_rate = param_get_integer(SOUND_CAPTURE_GROUP, "alsa_sampling_rate");
	const char *data_dir = param_get_string(SOUND_CAPTURE_GROUP, "data_dir");
	const char *file_dir_format = param_get_string_with_default(SOUND_CAPTURE_GROUP, "file_dir_format", FILE_DIR_FORMAT);
	const char *file_name_format = param_get_string_with_default(SOUND_CAPTURE_GROUP, "file_name_format", FILE_NAME_FORMAT);
	const char *file_ext = param_get_string(SOUND_CAPTURE_GROUP, "sound_file_ext");
	const char *details_ext = param_get_string(SOUND_CAPTURE_GROUP, "sound_details_ext");
	char *simulate_input_from_file = param_get_string(SOUND_CAPTURE_GROUP, "simulate_input_from_file");
	soundfile_t	*simulated_input = NULL;
	
	if (simulate_input_from_file && strlen(simulate_input_from_file))
		simulated_input = soundfile_open_read(simulate_input_from_file);
	
	// if a duration is given, then set time_limit to (current time + duration), 
	// otherwise set time_limit to max value of time_t (signed long) so it'll never be reached
	time_t time_limit = duration ? time(NULL) + duration : LONG_MAX;

	if (ensure_directory_exists(data_dir, ".", 20))
			exit(1);

	int16_t *buffer = salloc(n_channels*buffer_frames*sizeof *buffer); 
    struct sigaction reapchildren = {{0}};
	reapchildren.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &reapchildren, 0);

	if (!simulated_input) {
		do_alsa_init(param_get_string(SOUND_CAPTURE_GROUP, "alsa_pcm_name"),
				param_get_integer(SOUND_CAPTURE_GROUP, "alsa_sampling_rate"),
				param_get_integer(SOUND_CAPTURE_GROUP, "alsa_n_periods"),
				param_get_integer(SOUND_CAPTURE_GROUP, "alsa_periods_size"),
				param_get_integer(SOUND_CAPTURE_GROUP, "alsa_n_channels"),
				param_get_integer(SOUND_CAPTURE_GROUP, "alsa_buffer_size"));
	} 
	
//	int beep_enabled = param_get_boolean(SOUND_CAPTURE_GROUP, "beep");
	dp(30, "starting loop\n");
	for (int i = 0; time(NULL) < time_limit; ++i) {
		dp(30, "loop %d\n", i);
		int length;
		if (simulated_input) {
			length = soundfile_read(simulated_input, buffer, buffer_frames);
			dp(20, "%d frames read from %s (%d requested)\n", length, simulate_input_from_file, buffer_frames);
			if (length <buffer_frames) {
				dp(2, "existing because insufficient simulated input left to fill buffer\n");
				exit(0);
			}
		} else {
			length = alsa_readi(buffer, buffer_frames);
		}
		gettimeofday(&tv, NULL);
		time_t t = time(NULL);
		struct tm *local = localtime(&t);
		if (length <= 0)
			continue;
//		if (beep_enabled) {
//			beep_enabled = FALSE;
//			int active_high = param_get_boolean(SOUND_CAPTURE_GROUP, "active_high");
//			beep(1, 1000, 1, active_high);
//			msleep(300);
//			beep(1, 2000, 1, active_high);
//			msleep(300);
//			beep(1, 3000, 1, active_high);
//		}
		char file_dir[PATH_MAX];
		snprintf(file_dir, sizeof(file_dir), file_dir_format, 1900 + local->tm_year, local->tm_mon+1, local->tm_mday);
		if (ensure_directory_exists(data_dir, file_dir, 20))
			exit(1);

		char details_pathname[PATH_MAX];
		char pathname[PATH_MAX];
		snprintf(details_pathname, sizeof(details_pathname), file_name_format, data_dir, file_dir, local->tm_hour, local->tm_min, local->tm_sec, (uint32_t)tv.tv_usec, details_ext);
		snprintf(pathname, sizeof(pathname), file_name_format, data_dir, file_dir, local->tm_hour, local->tm_min, local->tm_sec, (uint32_t)tv.tv_usec, file_ext);
		write_data(buffer, n_channels, length, sampling_rate, pathname, details_pathname, tv.tv_sec, (uint32_t)tv.tv_usec);
        if (length < buffer_frames)
			die("too few frames returned(%d) - %d requested", length, buffer_frames);
            
	}
}



/* ensure the directory exists and is writable. 
 * return 0 on success
 * return -1 if parent doesn't exist
 * return -2 if parent is not a directory (is a file or fifo or something
 * 	[symlink to a directory is okay])
 * return -3 if parent is not writable
 * return -4 if mkdir failed (consult errno to see why)
 * return -5 if directory is not writable
 * NOTE: ideally this should have a single parameter (the full pathname), but
 * dirname & basename are troublesome - two versions of basename, etc */
int
ensure_directory_exists(const char *parent, const char *dir_name, int debug_level)
{
	uid_t uid = getuid();
   	gid_t gid = getgid();
	struct stat st;

	char dir[PATH_MAX];
	snprintf(dir, sizeof(dir), "%s/%s", parent, dir_name);

	// test if dir already exists and is writable
	if (stat(dir, &st) == 0) {
	   	if (have_write_permission(st, uid, gid)) {
			return 0;
		} else {
			dp(debug_level, "Directory is not writable: '%s'\n", dir);
			return -5;
		}
	}

	// see if parent exists
	errno = 0;
	if (stat(parent, &st) != 0) {
		dp(debug_level, "Parent directory not found '%s': %s\n", parent, strerror(errno));
		return -1;
	}
	
	// ensure that parent is actually a directory
	if (!S_ISDIR(st.st_mode)) {
		dp(debug_level, "Parent path is not a directory: '%s'\n", parent);
		return -2;
	}

	// see if we have write permission to parent
	if (!have_write_permission(st, uid, gid)) {
		dp(debug_level, "Parent directory is not writable: '%s'\n", parent);
		return -3;
	}

	// attempt to create directory
	errno = 0;
	if (mkdir(dir, (mode_t)0777)) {
		dp(debug_level, "Couldn't create directory '%s': %s\n", dir, strerror(errno));
		return -4;
	}

	// check we can have write permission to new directory
	errno = 0;
	if (stat(dir, &st) != 0) {
		dp(debug_level, "Couldn't newly created directory '%s': %s\n", dir, strerror(errno));
		return -4;
	}

	if (!have_write_permission(st, uid, gid)) {
		dp(debug_level, "Newly created directory is not writable: '%s'\n", dir);
		return -5;
	}

	return 0;
}

int
have_write_permission(struct stat st, uid_t uid, gid_t gid)
{
	return ((st.st_mode & S_IWOTH) // others have write
			|| (st.st_mode & S_IWGRP && st.st_gid == gid) // we are the right group and group has write
			|| (st.st_mode & S_IWUSR && st.st_uid == uid));  // we are the right user and user has write
}

	
void
write_data(int16_t *buffer, int n_channels, int n_frames, int sampling_rate, char *pathname, char *details_pathname, time_t seconds, uint32_t microseconds) {
	char wav_header[WAV_HEADER_SIZE];
	set_16bit_WAV_header(wav_header, n_frames, n_channels, sampling_rate);
	dp(30, "pathname=%s details_pathname=%s\n", pathname, details_pathname);
	unlink(details_pathname);
	unlink(pathname);
	switch (param_get_integer(SOUND_CAPTURE_GROUP, "sound_compression_type")) {
		case 0:
			write_wav_data(wav_header, buffer, n_channels, n_frames, sampling_rate, pathname);
			break;
		case 1:
			write_wavpack_data(wav_header, buffer, n_channels, n_frames, sampling_rate, pathname);
			break;
		case 2:
			fork_write_cmd(wav_header, buffer, n_channels, n_frames, sampling_rate, pathname, write_externally_compressed_data);
			break;
		default:
			die("unknown compression type '%d' requested", param_get_integer(SOUND_CAPTURE_GROUP, "sound_compression_type"));
	}
	FILE *fp = fopen(details_pathname, "w");
	assert(fp);
	fprintf(fp, "%lu.%06lu@%02d\n", (unsigned long)seconds, (unsigned long)microseconds, 10*calculate_monitoring_priority(seconds));
	fclose(fp);
}


void
write_wav_data(char *wav_header, int16_t *buffer, int n_channels, int n_frames, int sampling_rate, char *pathname)
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
write_externally_compressed_data(char *wav_header, int16_t *buffer, int n_channels, int n_frames, int sampling_rate, char *pathname)
{
	char *compressor = param_get_string(SOUND_CAPTURE_GROUP, "sound_compressor_shellcmd");
	if (!compressor && strlen(compressor) == 0) {
		dp(1, "external compressor requested, but not configured");
		return;
	}
	char command[PATH_MAX+256];
	snprintf(command, sizeof command, compressor, pathname);
	dp(20, "command='%s'\n", command);
	FILE *f = popen(command, "w");
	assert(f);
	if (fwrite(wav_header, WAV_HEADER_SIZE, 1, f) != 1)
		die("fwrite header failed");
	if (fwrite(buffer, sizeof *buffer, n_channels*n_frames, f) != n_channels*n_frames) 
		die("fwrite data failed");
	fclose(f);

}


// popen/pclose is not c99
extern FILE *popen(const char *command, const char *type); 
extern int pclose(FILE *stream);

void
fork_write_cmd(char *wav_header, int16_t *buffer, int n_channels, int n_frames, int sampling_rate, char *pathname, void write_cmd(char *, int16_t *, int, int, int, char *))
{
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
	dp(20,"child starts\n");
	for (int i=3; i < 30;i++)
		close(i);

	write_cmd(wav_header, buffer, n_channels, n_frames, sampling_rate, pathname);

	dp(20,"child complete\n");
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
