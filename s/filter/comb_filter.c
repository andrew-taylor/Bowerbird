#include "i.h"

void comb(char *in_filename, char *out_filename) {
	soundfile_t	*infile = soundfile_open_read(in_filename);
	if (!infile) sdie(NULL, "can not open input file %s: ", in_filename) ;
	soundfile_t	*outfile = soundfile_open_write(out_filename, infile->channels, infile->samplerate);
	if (!outfile) sdie(NULL, "can not open output file %s: ", out_filename) ;
	
	int n_channels = infile->channels;
	const int period = infile->samplerate/get_option_double("comb_filter_frequency");
	const double feedback_weight = get_option_double("comb_filter_feedback_weight");
	dp(10, "comb_filter_period = %d\n", period);
	int nFrames;
	int buffer_size = 8192;
	sample_t sample[buffer_size][n_channels];
	int ringBuffer[period][n_channels];
	int ringBufferIndex = 0;;
	memset(ringBuffer, 0, sizeof ringBuffer);
	double f = 1;
	int stablizing_iterations = 1/feedback_weight;
	int iterations = 0;
	while ((nFrames = soundfile_read(infile, &sample[0][0], buffer_size)) > 0) {
		sample_t filtered_sample[nFrames][n_channels];
		for (int frame = 0; frame < nFrames;  frame++) {
			int *r = &ringBuffer[ringBufferIndex][0];
			for (int i = 0; i < n_channels; i++)
				filtered_sample[frame][i] = sample[frame][i] - r[i];
			
			for (int i = 0; i < n_channels; i++)
				r[i] = r[i] *(1-f) + sample[frame][i] * f;
	//			r[i] = 0;
			ringBufferIndex++;
			if (ringBufferIndex == period) {
				ringBufferIndex = 0;
				iterations++;
				if (iterations >= stablizing_iterations)
					f = feedback_weight;
				else
					f = (feedback_weight*stablizing_iterations)/iterations;	
				assert(f <= 1);
			}
		}
		if (!soundfile_write(outfile, &filtered_sample[0][0], nFrames))
			sdie(outfile, "sf_writef_int returned %d (expected %d): ", framesWritten, nFrames);
	}
	soundfile_close(outfile);
	soundfile_close(infile);
}
