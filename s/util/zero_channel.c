#include "i.h"

int
main(int argc, char *argv[]) {
	int optind = initialize(argc, argv, "util", "0.1.1", "<input-sound-file> <output-sound-file>");
	if (argc - optind != 2)
		die("Usage: <input-sound-file> <output-sound-file>\n");
	uint32_t zero_channel_bitmap = param_get_integer_with_default("util", "zero_channel_bitmap", 0xAAAAAAA);
	silence_removal_file(argv[optind], argv[optind+1], zero_channel_bitmap);
	return 0;
}

void
silence_removal_file(char *infilename, char *outfilename, uint32_t zero_channel_bitmap) {
	soundfile_t	*infile = soundfile_open_read(infilename);
	if (!infile) sdie(NULL, "can not open input file %s: ", infilename) ;
	soundfile_t	*outfile = soundfile_open_write(outfilename, infile->channels, infile->samplerate);
	if (!outfile) sdie(NULL, "can not open output file %s: ", outfilename) ;
	uint32_t buffer_frames = param_get_integer_with_default("util", "buffer_frames", 4096);
	int n_channels = infile->channels;
	sample_t samples_buffer[buffer_frames][n_channels];
    index_t frames_written = 0;
    while (frames_written < infile->frames) {
        int n_frames = soundfile_read(infile, (sample_t *)samples_buffer, buffer_frames);
        if (n_frames <= 0)
            die("soundfile_read returned insufficient frames");
        dp(33, "n_frames=%d\n", n_frames);
		for (int channel = 0; channel < n_channels; channel++) {
            if (zero_channel_bitmap & (1 << channel))
                for (int f = 0; f < n_frames; f++)
                    samples_buffer[f][channel] = 0;
		}
        soundfile_write(outfile, samples_buffer[0], n_frames);
        frames_written += n_frames;
	}
	soundfile_close(infile);
	soundfile_close(outfile);
}
