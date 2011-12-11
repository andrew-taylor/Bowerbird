#include "i.h"

void comb(char *in_filename, char *out_filename) {
	soundfile_t	*infile = soundfile_open_read(in_filename);
	if (!infile) sdie(NULL, "can not open input file %s: ", in_filename) ;
	soundfile_t	*outfile = soundfile_open_write(out_filename, infile->channels, infile->samplerate);
	if (!outfile) sdie(NULL, "can not open output file %s: ", out_filename) ;
	
	int n_channels = infile->channels;
	double track_width = param_get_double_with_default(FILTER_GROUP, "comb_filter_track_width", 0.01);
	double sum_of_squares_decay = param_get_double_with_default(FILTER_GROUP, "comb_filter_sum_of_squares_decay", 0.99);
	double slew_rate = param_get_double_with_default(FILTER_GROUP, "comb_filter_slew_rate", 0.0001);
	const double comb_frequency = param_get_double_with_default(FILTER_GROUP, "comb_filter_frequency", 4000);
	const double comb_frequency_min = param_get_double_with_default(FILTER_GROUP, "comb_filter_frequency_min", comb_frequency*0.9);
	const double comb_frequency_max = param_get_double_with_default(FILTER_GROUP, "comb_filter_frequency_max", comb_frequency*1.1);
	double comb_delay = infile->samplerate/comb_frequency;
	dp(10, "infile->samplerate = %f comb_frequency=%fcomb_frequency comb_delay = %f\n", (float)infile->samplerate, comb_frequency, comb_delay);
	const double comb_delay_min = infile->samplerate/comb_frequency_max;
	const double comb_delay_max = infile->samplerate/comb_frequency_min;
	double comb_delay_low = comb_delay*(1-track_width);
	double comb_delay_high = comb_delay*(1+track_width);
	dp(10, "comb_delay=%f  comb_delay_low=%f comb_delay_high=%f  comb_delay_min=%f comb_delay_max=%f \n", comb_delay, comb_delay_low, comb_delay_high, comb_delay_min, comb_delay_max);
	const double feedback_weight = param_get_double_with_default(FILTER_GROUP, "comb_filter_feedback_weight", 0.04);
	int buffer_size = 8192;
	int ring_buffer_needs = (int)(comb_delay_max+2);
	if (buffer_size < ring_buffer_needs)
		buffer_size = ring_buffer_needs;
	sample_t sample[buffer_size][n_channels];
	index_t stablizing_frames =  param_get_integer_with_default(FILTER_GROUP, "comb_stablizing_frames", (int)(ring_buffer_needs/feedback_weight));
	dp(10, "buffer_size=%d stablizing_frames=%d\n", (int)buffer_size, (int)stablizing_frames);
	sample_t ring_buffer[buffer_size][n_channels];
	index_t frame_count = soundfile_read(infile, &ring_buffer[0][0], ring_buffer_needs);
	if (frame_count < ring_buffer_needs) {
		soundfile_write(outfile, &ring_buffer[0][0], frame_count);
		soundfile_close(outfile);
		soundfile_close(infile);
		return;
	}
	int nFrames;
	double sum_of_squares[] = {0, 0, 0};
	while ((nFrames = soundfile_read(infile, &sample[0][0], buffer_size)) > 0) {
		sample_t filtered_sample[nFrames][n_channels];
		dp(10, "frame_count=%lld comb_delay = %f  comb_delay_low = %f comb_delay_high = %f\n", (long long)frame_count, comb_delay, comb_delay_low, comb_delay_high);
		for (int frame = 0; frame < nFrames;  frame++) {
			double feedback;
			if (frame_count < stablizing_frames)
				feedback = (feedback_weight*frame_count)/stablizing_frames;	
			else
				feedback = feedback_weight;
			double delays[] = {comb_delay, comb_delay_low, comb_delay_high};
			for (int d = 0; d < 3; d++) {
				double delay = delays[d];
				double sample_index = frame_count-delay;
				index_t i1 = (index_t)sample_index;
				double fraction = sample_index-i1;
				index_t i2 = i1 + 1;
				i1 %= buffer_size;
				i2 %= buffer_size;
				int ring_buffer_index = frame_count % buffer_size;
				if (d == 0) dp(20, "sample_index=%f fraction=%f i1=%d i2=%d ring_buffer_index=%d frame_count=%d\n", sample_index, fraction, i1, i2, (int)ring_buffer_index, (int)frame_count);
				double squares = 0;
				for (int c = 0; c < n_channels; c++) {
					double delayed_sample = ring_buffer[i1][c]*(1-fraction) + ring_buffer[i2][c]*fraction;
					double delta =  sample[frame][c] - delayed_sample;
					dp(20, "d=%d c=%d delayed_sample=%f(%d,%d)  sample[frame][c]=%d delta=%f\n", d, c, delayed_sample, ring_buffer[i1][c], ring_buffer[i2][c], (int)sample[frame][c], delta);
					squares += delta*delta;
					if (d == 0) {
						filtered_sample[frame][c] = delta;
						ring_buffer[ring_buffer_index][c] =  feedback*delayed_sample + (1-feedback)*sample[frame][c];
					}
				}
				sum_of_squares[d] = sum_of_squares_decay*sum_of_squares[d]+(1-sum_of_squares_decay)*(squares);
			}
			frame_count++;
			if (sum_of_squares[1] > 0) {
				double adjust;
				if (sum_of_squares[1] < sum_of_squares[2]) {
					double factor = (sum_of_squares[2]*sum_of_squares[0])/(sum_of_squares[1]*sum_of_squares[1]);
					if (factor > 1) factor = 1;
					adjust = 1-factor*slew_rate;
				} else {
					double factor = (sum_of_squares[1]*sum_of_squares[0])/(sum_of_squares[2]*sum_of_squares[2]);
					if (factor > 1) factor = 1;
					adjust = 1+factor*slew_rate;
				}	
				comb_delay *= adjust;
				dp(30, "sum_of_squares %f low %f high %f adjust=%f comb_delay=%f\n", sum_of_squares[0], sum_of_squares[1], sum_of_squares[2], adjust, comb_delay);
				if (comb_delay < comb_delay_min) comb_delay = comb_delay_min;
				if (comb_delay > comb_delay_max) comb_delay = comb_delay_max;
				comb_delay_low = comb_delay*(1-track_width);
				comb_delay_high = comb_delay*(1+track_width);
			}
		}
//		if (sum_of_squares[2] < sum_of_squares[1]) {
//			if (sum_of_squares[2] < sum_of_squares[0])
//				comb_delay *= (1+slew_rate);
//		} else if (sum_of_squares[1] < sum_of_squares[0]) 
//			comb_delay *= (1-slew_rate);
		soundfile_write(outfile, &filtered_sample[0][0], nFrames);
	}
	soundfile_close(outfile);
	soundfile_close(infile);
}
