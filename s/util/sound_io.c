#include "i.h"
#include <wavpack/wavpack.h>

void
sdie(SNDFILE *sf, char *format,  ...) {
	va_list ap;
	if (myname)
		fprintf(stderr, "%s: ", myname);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	if (sf)
		sf_perror(sf);
	if (verbosity > 9)
		abort();
	exit(1);
}

void
soundfile_die(soundfile_t *sf, char *format,  ...) {
	va_list ap;
	if (myname)
		fprintf(stderr, "%s: ", myname);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	if (sf && sf->t == sft_libsndfile)
		sf_perror(sf->p);
	if (verbosity > 9)
		abort();
	exit(1);
}

soundfile_t *
soundfile_open_read(const char *path) {
	dp(30, "path=%s \n", path);
	soundfile_t *s = salloc(sizeof *s);
	s->m = sft_read;
	if 	(g_regex_match_simple ("\\.wv$", path, 0, 0)) {
		char error[80] = {0};
	    int flags = 0;
	    int norm_offset = 0;
		s->t = sft_wavpack;
	    s->p = WavpackOpenFileInput(path, error, flags, norm_offset);
		if (!s->p)
			die("can not open input file '%s'", path);
		s->bits_per_sample = WavpackGetBitsPerSample(s->p);
		s->channels = WavpackGetNumChannels(s->p);
		s->samplerate = WavpackGetSampleRate(s->p);
		s->frames = WavpackGetNumSamples(s->p);
	} else {
		SF_INFO	 	infile_info = {0};
		if (strcmp(path, "-"))
			s->p = sf_open(path, SFM_READ, &infile_info);
		else
			s->p = sf_open_fd(0, SFM_READ, &infile_info, 0);
		if (!s->p)
			die("can not open input file '%s'", path);
		s->t = sft_libsndfile;
		s->channels = infile_info.channels;
		s->samplerate = infile_info.samplerate;
		s->frames = infile_info.frames;
	}
	return s;
}

// Next 2 functions from tinypack.c in wavpack distribution
//              Copyright (c) 1998 - 2007 Conifer Software.               //
//                          All Rights Reserved.                          //
//      Distributed under the BSD Software License (see license.txt)      //

static int
DoWriteFile (FILE *hFile, void *lpBuffer, uint32_t nNumberOfBytesToWrite, uint32_t *lpNumberOfBytesWritten)
{
    uint32_t bcount;

    *lpNumberOfBytesWritten = 0;

    while (nNumberOfBytesToWrite) {
        bcount = fwrite ((uchar *) lpBuffer + *lpNumberOfBytesWritten, 1, nNumberOfBytesToWrite, hFile);

        if (bcount) {
            *lpNumberOfBytesWritten += bcount;
            nNumberOfBytesToWrite -= bcount;
        }
        else
            break;
    }

    return !ferror (hFile);
}

static int
write_block (void *id, void *data, int32_t length)
{
	dp(30, "id=%p data=%p length=%d\n", id, data, length);
    soundfile_t *wid = id;
    uint32_t bcount;

    if (wid->error)
        return FALSE;

    if (wid && wid->file && data && length) {
        if (!DoWriteFile (wid->file, data, length, &bcount) || bcount != length) {
            fclose (wid->file);
            wid->file = NULL;
            wid->error = 1;
            return FALSE;
        }
        else {
            wid->bytes_written += length;

            if (!wid->first_block_size)
                wid->first_block_size = bcount;
        }
    }

    return TRUE;
}

soundfile_t *
soundfile_open_write(const char *path, int n_channels, double sampling_rate) {
	dp(30, "path=%s n_channels=%d sampling_rate=%g\n", path, n_channels, sampling_rate);
	soundfile_t *s = salloc(sizeof *s);
	s->m = sft_write;
	s->channels = n_channels;
	s->samplerate = sampling_rate;
	if 	(g_regex_match_simple ("\\.wv$", path, 0, 0)) {
		s->t = sft_wavpack;
		s->file = fopen(path, "w+b");
		if (!s->file)
			die("can not open output file '%s'", path);
		WavpackContext *wpc = s->p = WavpackOpenFileOutput(write_block, s, NULL);
		if (!s->p)
			die("can not open input file '%s'", path);
  		WavpackConfig config = {0};
		config.bytes_per_sample = 2;
		s->bits_per_sample = config.bits_per_sample = 16;
		config.channel_mask = n_channels == 1 ? 4 : 3;
		config.num_channels = n_channels;
		config.sample_rate = sampling_rate;
		if (!WavpackSetConfiguration(wpc, &config, -1))
        	die("WavpackSetConfiguration failed: %s\n", WavpackGetErrorMessage(wpc));
		if (!WavpackPackInit(wpc))
        	die("WavpackPackInit failed: %s\n", WavpackGetErrorMessage(wpc));
			
	} else {
		SF_INFO	 	outfile_info = {0};
		outfile_info.samplerate = sampling_rate;
		outfile_info.channels = n_channels;
		outfile_info.format = SF_FORMAT_PCM_16;
		
		if (g_regex_match_simple ("\\.flac$", path, 0, 0))
			outfile_info.format |= SF_FORMAT_FLAC;
		else
			outfile_info.format |= SF_FORMAT_WAV;
		if (strcmp(path, "-"))
			s->p = sf_open(path, SFM_WRITE, &outfile_info);
		else
			s->p = sf_open_fd(1, SFM_WRITE, &outfile_info, 0);  // write to stdout if name is "-"
		if (!s->p)
			die("can not open output file '%s'", path);
		s->t = sft_libsndfile;
	}
	return s;
}

index_t
soundfile_read(soundfile_t *sf, sample_t *buffer, index_t n_frames) {
	if (sf->t == sft_libsndfile) {
		return SF_READF_SAMPLE_T(sf->p, buffer, n_frames);
	} else {
		int buffer_size = 65536;
		int32_t b[buffer_size*sf->channels];
		index_t frames_read = 0;
		double divisor = 1L << (sf->bits_per_sample - 1);
		while (1) {
			int to_read = MIN(buffer_size, n_frames - frames_read);
			if (to_read <= 0)
				return frames_read;
			int n = WavpackUnpackSamples(sf->p, b, to_read);
			if (n < 0 || (n == 0 && frames_read == 0))
				die("WavpackUnpackSamples returned %d", n);
			if (n == 0)
				return frames_read;
			for (index_t f = 0; f < n; f++) {
				for (int c = 0; c < sf->channels; c++)
					buffer[frames_read*sf->channels+c] = double_to_sample_t(b[f*sf->channels+c]/divisor);
				frames_read++;
			}
		}
	}
}

index_t
soundfile_read_double(soundfile_t *sf, double *buffer, index_t n_frames) {
	if (sf->t == sft_libsndfile) {
		return sf_read_double(sf->p, buffer, n_frames);
	} else {
		int buffer_size = 65536;
		int32_t b[buffer_size*sf->channels];
		index_t frames_read = 0;
		double divisor = 1L << (sf->bits_per_sample - 1);
		while (1) {
			int to_read = MIN(buffer_size, n_frames - frames_read);
			if (to_read <= 0)
				return frames_read;
			int n = WavpackUnpackSamples(sf->p, b, to_read);
			if (n < 0 || (n == 0 && frames_read == 0))
				die("WavpackUnpackSamples returned %d", n);
			if (n == 0)
				return frames_read;
			for (index_t f = 0; f < n; f++) {
				for (int c = 0; c < sf->channels; c++)
					buffer[frames_read*sf->channels+c] = b[f*sf->channels+c]/divisor;
				frames_read++;
			}
		}
	}
}

void
soundfile_write(soundfile_t *sf, sample_t *buffer, index_t n_frames) {
	dp(30, "sf=%p buffer=%p n_frames=%d\n", sf, buffer, n_frames);
	if (sf->t == sft_libsndfile) {
		sf_count_t count; 
		if ((count = SF_WRITEF_SAMPLE_T(sf->p, buffer, n_frames)) != n_frames)
			sdie(sf->p, "sf_writef_double returned %d (expected %d): ", count, n_frames);
	} else {
		WavpackContext *wpc = sf->p;
		int32_t sample_buffer[n_frames*sf->channels];
		for (int i = 0; i < 10; i++)
			dp(30, "buffer[%d]=%g\n", i, (double)buffer[i]);
		
		double multiplier = 1L << (sf->bits_per_sample - 1);
		for (int i = 0; i < n_frames*sf->channels; i++)
  	  		sample_buffer[i] = multiplier*sample_t_to_double(buffer[i]); // FIXME may overflow buffer[1] == 1
		if (!WavpackPackSamples(sf->p, sample_buffer, n_frames))
            die("WavpackPackSamples failed: %s\n", WavpackGetErrorMessage(wpc));
	}
}

void
soundfile_write_double(soundfile_t *sf, double *buffer, index_t n_frames) {
	if (sf->t == sft_libsndfile) {
		sf_count_t count; 
		if ((count = sf_writef_double(sf->p, buffer, n_frames)) != n_frames)
			sdie(sf->p, "sf_writef_double returned %d (expected %d): ", count, n_frames);
	} else {
		WavpackContext *wpc = sf->p;
		int32_t sample_buffer[n_frames*sf->channels];
		for (int i = 0; i < 10; i++)
			dp(30, "buffer[%d]=%g\n", i, (double)buffer[i]);
		
		double multiplier = 1L << (sf->bits_per_sample - 1);
		for (int i = 0; i < n_frames*sf->channels; i++)
  	  		sample_buffer[i] = multiplier*buffer[i]; // FIXME may overflow buffer[1] == 1
		if (!WavpackPackSamples(sf->p, sample_buffer, n_frames))
            die("WavpackPackSamples failed: %s\n", WavpackGetErrorMessage(wpc));
	}
}

void
soundfile_close(soundfile_t *sf) {
	dp(30, "sf=%p \n", sf);
	if (sf->t == sft_libsndfile) {
		sf_close(sf->p);
	} else {
		if (sf->m == sft_write) {
		    if (!WavpackFlushSamples(sf->p))
           		die("WavpackFlushSamples failed: %s\n", WavpackGetErrorMessage(sf->p));
			WavpackCloseFile(sf->p);
			fclose(sf->file);
		}
	}
}

void
soundfile_write_entire_double(char *filename, index_t n_frames, int n_channels, double sampling_rate, double *samples) {
	soundfile_t *s = soundfile_open_write(filename, n_channels, sampling_rate);
	soundfile_write_double(s, samples, n_frames);
	soundfile_close(s);
}

void
soundfile_write_entire(char *filename, index_t n_frames, int n_channels, double sampling_rate, sample_t *samples) {
	soundfile_t *s = soundfile_open_write(filename, n_channels, sampling_rate);
	soundfile_write(s, samples, n_frames);
	soundfile_close(s);
}

sample_t *
soundfile_read_entire(char *filename) {
	soundfile_t *s = soundfile_open_read(filename);
	sample_t *d = salloc(s->frames*s->channels*sizeof d[0]);
	if (soundfile_read(s, d, s->frames) != s->frames)
		die("soundfile_read_double returned insufficient frames");
	soundfile_close(s);
	return d;
}

/**
 * Read an entire  sound file into memory as doubles
 * @param[in] filename file to read
 * @param[out] n_frames set to number of frames in filename 
 * @param[out] n_channels set to number of channels in filename 
 * @returns a pointer to malloc'ed buffer of n_frames*n_channels doubles
 *
 * @note calls die() if operation fails -
 */
double *
soundfile_read_entire_double(char *filename, index_t *n_frames, int *n_channels, double *sampling_rate) {
	soundfile_t *s = soundfile_open_read(filename);
	*n_frames = s->frames;
	*n_channels = s->channels;
	*sampling_rate = s->samplerate;
	double *d = salloc(s->frames*s->channels*sizeof d[0]);
	if (soundfile_read_double(s, d, s->frames) != s->frames)
		die("soundfile_read_double returned insufficient frames");
	soundfile_close(s);
	return d;
}

#ifdef OLD
sample_t *
read_sound_file(char *filename, index_t *n_frames, int *n_channels, double *sampling_rate) {
	SF_INFO	 	infile_info = {0};
	SNDFILE	 	*infile = sf_open(filename, SFM_READ, &infile_info);
	if (!infile) sdie(NULL, "can not open input file %s: ", filename) ;
	*n_frames = infile_info.frames;
	*n_channels = infile_info.channels;
	*sampling_rate = infile_info.samplerate;
	sample_t *d = salloc(*n_frames**n_channels*sizeof d[0]);
	if (SF_READF_SAMPLE_T(infile, d, *n_frames) !=  *n_frames)
		die("sf_readf_double returned insufficient frames");
	sf_close(infile);
	return d;
}

double *
read_sound_file_double(char *filename, index_t *n_frames, int *n_channels, double *sampling_rate) {
	SF_INFO	 	infile_info = {0};
	SNDFILE	 	*infile = sf_open(filename, SFM_READ, &infile_info);
	if (!infile) sdie(NULL, "can not open input file %s: ", filename) ;
	*n_frames = infile_info.frames;
	*n_channels = infile_info.channels;
	*sampling_rate = infile_info.samplerate;
	double *d = salloc(*n_frames**n_channels*sizeof d[0]);
	if (sf_readf_double(infile, d, *n_frames) !=  *n_frames)
		die("sf_readf_double returned insufficient frames");
	sf_close(infile);
	return d;
}


void
write_wav_file(char *filename, index_t n_frames, int n_channels, double sampling_rate, sample_t *samples) {
	SF_INFO outfile_info;
	outfile_info.samplerate = sampling_rate;
	outfile_info.channels = n_channels;
	outfile_info.format = SF_FORMAT_WAV|SF_FORMAT_PCM_16;
	SNDFILE	*outfile;
	if (!(outfile = sf_open(filename, SFM_WRITE, &outfile_info)))
		sdie(NULL, "can not open output file %s: ", filename);
	sf_count_t count; 
	if ((count = SF_WRITEF_SAMPLE_T(outfile, samples, n_frames)) != n_frames)
		sdie(outfile, "sf_writef_double returned %d (expected %d): ", count, n_frames);
	sf_close(outfile);
}

static const int buffer_size = 4096;

static SNDFILE *current_snd_file = NULL;
static int n_channels  = 0;
static int *buffer;
static int buffer_start  = 0;
static int buffer_finish  = 0;

SNDFILE *
bsf_open(const char *path, int mode, SF_INFO *sfinfo) {
	if (current_snd_file != NULL)
		die("bsp_open called when already  in use");
	current_snd_file = sf_open(path, mode, sfinfo);
	buffer_start = 0;
	buffer_finish = 0;
	n_channels = sfinfo->channels;
	buffer = salloc(n_channels*buffer_size*sizeof *buffer);
	return current_snd_file;
}

sf_count_t
bsf_readf_int(SNDFILE *sndfile, int *ptr, sf_count_t frames) {
	if (sndfile != current_snd_file)
		die("bsf_readf_int for different SNDFILE to bsf_open");
		
	if (buffer_start >= buffer_finish) {
		int count = sf_readf_int(sndfile, buffer, buffer_size);
		if (count <= 0)
			return count;
		buffer_start = 0;
		buffer_finish = count;
	}
	int frames_from_buffer = buffer_finish - buffer_start;
	if (frames_from_buffer > frames)
		frames_from_buffer = frames;
	memcpy(ptr, &buffer[buffer_start*n_channels], frames_from_buffer*n_channels*sizeof *buffer);
	buffer_start += frames_from_buffer;
	if (frames_from_buffer == frames)
		return frames;
	int frames_read = sf_readf_int(sndfile, ptr+frames_from_buffer*n_channels, frames-frames_from_buffer);
	if (frames_read < 0)
		return frames_read;
	return frames_from_buffer + frames_read;
}

int
bsf_close(SNDFILE *sndfile) {
	if (sndfile != current_snd_file)
		die("bsf_close for different SNDFILE to bsf_open");
	dprintf(31, "bfclose(%x) buffer=%p\n", (unsigned)sndfile,  buffer);
	g_free(buffer);
	current_snd_file = NULL;
	return sf_close(sndfile);
}
#endif
