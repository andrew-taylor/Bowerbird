#include "i.h"

static void
check_sound_files_identical(char *file1, char *file2, double tolerance) {
	dp(30, "file1=%s file2=%s\n", file1, file2);
	soundfile_t *s1 =soundfile_open_read(file1);
	soundfile_t *s2 =soundfile_open_read(file2);
	assert(s1->channels == s2->channels);
	assert(s1->samplerate == s2->samplerate);
	assert(s1->frames == s2->frames);
	index_t channels = s1->channels;
	index_t frames = s1->frames;
	sample_t b1[frames*channels];
	sample_t b2[frames*channels];
	int n1 = soundfile_read(s1, b1, frames);
	assert(n1 == frames);
	int n2 = soundfile_read(s2, b2, frames);
	assert(n2 == frames);
	for (index_t i = 0; i < frames*channels; i++) {
		double denom = MAX(ABS(b1[i]),ABS(b2[i]));
		if (denom > 0 && ABS((b1[i] - b2[i])/denom) > tolerance)
			die("b1[%d] = %g b2[%d] = %g\n", i, (double)b1[i], i, (double)b2[i]);
	}
	soundfile_close(s1);
	soundfile_close(s2);
}

static void
copy_file(char *file1, char *file2) {
	dp(30, "file1=%s file2=%s\n", file1, file2);
	soundfile_t *s1 =soundfile_open_read(file1);
	soundfile_t *s2 =soundfile_open_write(file2, s1->channels, s1->samplerate);
	assert(s1->channels == s2->channels);
	assert(s1->samplerate == s2->samplerate);
	index_t channels = s1->channels;
	index_t frames = s1->frames;
	sample_t b1[frames*channels];
	int n1 = soundfile_read(s1, b1, frames);
	assert(n1 == frames);
	soundfile_write(s2, b1, frames);
	soundfile_close(s1);
	soundfile_close(s2);
}

int
main(int argc, char*argv[]) {
	int optind = testing_initialize(&argc, &argv, "");
	check_sound_files_identical(argv[optind], argv[optind+1], 1e-10);
	dp(0, "read OK\n");
	if (argc - optind > 2) {
		copy_file(argv[optind], argv[optind+2]);
		check_sound_files_identical(argv[optind], argv[optind+2], 0.001);
		dp(0, "write1 OK\n");
	}	
	if (argc - optind > 3) {
		copy_file(argv[optind], argv[optind+3]);
		check_sound_files_identical(argv[optind], argv[optind+3], 0.001);
		dp(0, "write2 OK\n");
	}	
	if (argc - optind > 4) {
		copy_file("-", argv[optind+4]);
		check_sound_files_identical(argv[optind], argv[optind+4], 0.001);
		dp(0, "read from stdin OK\n");
	}	
	if (argc - optind > 5) {
		copy_file(argv[optind], "-");
		check_sound_files_identical(argv[optind], argv[optind+5], 0.001);
		dp(0, "write to stdout OK\n");
	}	
	return 0;
}
