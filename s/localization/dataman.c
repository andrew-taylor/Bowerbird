/* these functions manage the audio data.
 *   o dataman_scan() should be called first
 *   o dataman_cleanup() should be called at the end
 */

#include "i.h"

/* config data */
char *base_dir;
char *date_dir;
char *station_dir[NUM_STATIONS];
double click_threshold;
int compress_clicktracks; 

/* they specify the same time, just in different formats */
struct tm *base_date; 
time_t     base_epoch;

/* internal data */
datafile_t files[NUM_STATIONS][MAX_FILES];
int num_files[NUM_STATIONS];

void dataman_cleanup(void)
{
	for (int i = 0; i<NUM_STATIONS; i++)
	for (int j = 0; j<num_files[i]; j++)
	{
		if (files[i][j].clicktrack != NULL)
			free(files[i][j].clicktrack);
	}
}

/* for qsort */
int file_compare(const void *p1, const void *p2)
{
	datafile_t *file1 = (datafile_t *)p1;
	datafile_t *file2 = (datafile_t *)p2;

	if (file1->time_at_eof == file2->time_at_eof)
		return 0;
	return (file1->time_at_eof > file2->time_at_eof)?1:-1;
}

/* for my binary search */
int file_consider(const void *p1, const void *p2)
{
	datafile_t *file = (datafile_t *)p1;
	double      time = *((double *)p2);

	if (time > file->time_at_eof)
		return 1;
	else if (time <= file->time_at_eof - 60)
		return -1;
	else
		return 0;
}

/* returns the index to an elem p in the array such that consider(p,key)==0 or else returns -1 */
int binary_search(void *array, size_t nmemb, size_t size, void *key, int(*consider)(const void*, const void *))
{
	if (nmemb == 0)
		return -1;

	int mididx = nmemb/2;
	void *mid = array + size*mididx;
	int result = consider(mid,key);
	if (result < 0)
	{
		return binary_search(array,mididx,size,key,consider);
	}
	else if (result > 0)
	{
		int recidx = binary_search(mid + size,nmemb-mididx-1,size,key,consider);
		if (recidx < 0)
			return -1;
		else
			return mididx+1+recidx;
	}
	else // result == 0
	{
		return mididx;
	}
}


/* scan the appropriate directories and populate the `files' arrays with datafile_t info */
void dataman_scan(void)
{
	for (int station = 0; station<NUM_STATIONS; station++)
	{
		num_files[station] = 0;	

		char dir[MAX_PATH_LEN];
		sprintf(dir,"%s/%s/%s",base_dir,station_dir[station],date_dir);

		DIR *dp;
		dp = opendir(dir);
		if (!dp) die("Failed to open the directory '%s'",dir);
		struct dirent *ep;
		while ((ep = readdir(dp)))
		{
			datafile_t *file = &files[station][num_files[station]];
			file->have_uncompressed = 0;
			file->have_compressed   = 0;

			char extension[16];
			if (sscanf(ep->d_name,"%lf@%d.%s",&file->time_at_eof,&file->suffix,(char *)&extension)==3)
			{
				// see if we have this file already
				int exists=0;
				for (int i=0; i<num_files[station]; i++)
				{
					if (approximately_zero(files[station][i].time_at_eof - file->time_at_eof))
					//if (files[station][i].time_at_eof == file->time_at_eof)
					{
						exists=1;
						file = &files[station][i];
						break;
					}
				}

				if (strcmp(extension,"wav")==0)
				{
					file->have_uncompressed = 1;
				}
				else if (strcmp(extension,"wv")==0)
				{
					file->have_compressed   = 1;
				}
				else continue;
		
				if (!exists)
				{
					// this is the first time we've found this file so set it up
					strncpy(file->base_filename,ep->d_name,MAX_FILENAME_LEN);
					// strip the extension
					char *pext = strrchr(ep->d_name,'.');
					file->base_filename[strlen(file->base_filename)-strlen(pext)]='\0';

					file->clicktrack = NULL;
					file->station = station;
					num_files[station]++;
				}
			}
		}
		closedir(dp);

		dprintf(5,"Found %d files for station %d on day %s\n",num_files[station],station,date_dir);

		// now we sort them (by their timestamps)
		qsort(&files[station],num_files[station],sizeof(datafile_t),file_compare);
	}

	// now we can set the base time
	base_epoch = floor(files[0][0].time_at_eof);
	base_date  = localtime(&base_epoch);
	dprintf(10,"Base epoch is %ld ",(long)base_epoch);
	char str[MAX_PATH_LEN];
	strftime(str,MAX_PATH_LEN,"%c",base_date);
	dprintf(10,"which is %s\n",str);
}

int sprint_filepath(char *output,datafile_t *file)
{
	char* extension = file->have_uncompressed ? ".wav" : ".wv";
	int compression = file->have_uncompressed ? UNCOMPRESSED : WAVPACK;
	sprintf(output,"%s/%s/%s/%s%s",base_dir,station_dir[file->station],date_dir,file->base_filename,extension);
	return compression;
}

/*============================================================================================*/

/* reads the raw audio data from the file, uncompressing it if necessary */

double *read_raw(char *filename,index_t *nsamples,int *nchannels,double *sampling_rate,int compression)
{
	double *data;
	if (compression == UNCOMPRESSED)
	{
		data = soundfile_read_entire_double(filename, nsamples, nchannels, sampling_rate);
	}
	else if (compression == WAVPACK)
	{
		char error[128]; // must be at least 80 chars
		WavpackContext *wavpack = WavpackOpenFileInput(filename,error,OPEN_NORMALIZE,0);
		*nchannels           = WavpackGetNumChannels(wavpack);
		*sampling_rate       = (double)WavpackGetSampleRate(wavpack);  // function returns a uint32_t
		*nsamples            = WavpackGetNumSamples(wavpack);

		// wavpack always returns the data in 4 byte chunks even if the wave file isn't 32-bit
		int32_t *buffer = salloc(4* (*nsamples) * (*nchannels));
		dprintf(10,"Uncompressing wavpack...");
		WavpackUnpackSamples(wavpack,buffer,*nsamples);
		dprintf(10,"done\n");

		index_t len = (*nsamples) * (*nchannels);
		if (WavpackGetMode(wavpack) & MODE_FLOAT)
		{
			data = (double *)buffer;
			dprintf(20,"read_raw: have 32-bit float wavpack file\n");
			dprintf(0,"The wavpack file used floats.. it hasn't been tested that these files are unpacked correctly.\n");
		}
		else
		{
			int bits_per_sample  = WavpackGetBitsPerSample(wavpack);
			dprintf(20,"read_raw: have %d bits per sample\n",bits_per_sample);
			double m = (0x1<<bits_per_sample)/2;
			data = salloc(sizeof(double) * len );
			for (index_t i=0; i<len; i++)
			{
				data[i] = ((double)buffer[i])/m;
			}
			free(buffer);
		}
		WavpackCloseFile(wavpack);
	}
	else
	{
		die("error: read_raw given unknown compression type");
	}

	return data;
}


/* write one channel of data to a file, compressing if desired.
 * note that this differs from read_raw in that the result of read_raw has
 * the data for all channels interleaved, whereas write_raw only deals with
 * a single channel */
 
void write_waveform(char *file, double *waveform, index_t nsamples, int compression)
{
	if (compression == UNCOMPRESSED)
	{
		soundfile_write_entire_double(file,nsamples,1,SAMPLING_RATE,waveform);
	}
	else
	{
		soundfile_write_entire_double("/tmp/clicktocompress.wav",nsamples,1,SAMPLING_RATE,waveform);
		char line[MAX_PATH_LEN];
		if (system("wavpack -y /tmp/clicktocompress.wav 2>/dev/null"))
		{
			printf("wavpack command failed for file '%s'\n", file);
			exit (1);
		}
		sprintf(line,"mv /tmp/clicktocompress.wv %s",file);
		if (system(line)) 
		{
			printf("failed to move compressed file to '%s'\n", file);
			exit(1);
		}
		/*
		printf("Writing compressed files isn't implemented yet!\n");
		exit(1);
		*/
/*		
  1. get a context and set block output function with WavpackOpenFileOutput()
  2. set the data format and specify modes with WavpackSetConfiguration()
  3. optionally write a RIFF header with WavpackAddWrapper()
  4. allocate buffers and prepare for packing with WavpackPackInit()
  5. actually compress audio and write blocks with WavpackPackSamples()
  6. flush final samples into blocks with WavpackFlushSamples()
  7. optionally write MD5 sum with WavpackStoreMD5Sum()
  8. optionally write RIFF trailer with WavpackAddWrapper()
  9. if MD5 sum or RIFF trailer written, call WavpackFlushSamples() again
  10. optionally append metadata tag with functions in next section
  11. optionally update number of samples with WavpackUpdateNumSamples()
  12. close the context with WavpackCloseFile()
*/
	}
}

/* read the specified region for all channels of the file.
 * if the region spans past the end of file, don't fail, just read to end of file.
 * returns the number of samples read.
 * output should be an array with one (double *) for each channel in the file, through
 * which the read waveforms will be returned (as newly allocated double*'s) */
index_t read_all_channels(double *output[], char *filename, index_t start, index_t len, int compressed)
{
	index_t nsamples;
	int nchannels;
	double sampling_rate;
	double *data = read_raw(filename,&nsamples,&nchannels,&sampling_rate,compressed);
	assert(sampling_rate == SAMPLING_RATE);
	assert(nchannels <= NUM_CHANNELS);
	if (start+len >= nsamples)
	{
		len = nsamples - start;
	}
	double (*channels)[nchannels] = (void *)data;
	for (int chan = 0; chan<nchannels; chan++)
	{
		output[chan] = salloc(len*sizeof(double));
		for (index_t i=0,j=start; i<len; i++,j++)
		{
			output[chan][i] = channels[j][chan];
		}
	}
	free(data);
	return len;
}	


/* reads a region (of `len' samples starting at sample `start') of channel `channel' 
 * of the wavfile `filename'.
 * 
 * (*output) points to a newly allocated waveform
 * returns the number of samples read
 */

index_t read_waveform(double **output, char *filename, int channel, index_t start, index_t len, int compressed)
{
	index_t nsamples;
	int nchannels;
	double sampling_rate;
//	double *data = soundfile_read_entire_double(filename,&nsamples,&nchannels,&sampling_rate);
	double *data = read_raw(filename,&nsamples,&nchannels,&sampling_rate,compressed);	
	assert(sampling_rate == SAMPLING_RATE);
	assert(nchannels > channel);
	assert(start >= 0);
	if (start+len >= nsamples)
	{
		/* we just read to the end of the file instead */
		dprintf(15,"read_waveform: wanting to read past end of file\n");
		len = nsamples - start;
	}

	*output = salloc(len*sizeof(double));
	double (*channels)[nchannels] = (void *)data;
	for (index_t i=0,j=start; i<len; i++,j++)
	{
		(*output)[i] = channels[j][channel];
	}
	free(data);

	dprintf(10,"Successfully read channel %d of waveform %s from %d to %d\n",channel,filename,start,start+len-1);
	return len;
}

/* reads a whole channel */

index_t read_whole_waveform(double **output, char *filename, int channel, int compression)
{
	int nchannels;
	index_t nsamples;
	double sampling_rate;
//	double *data = soundfile_read_entire_double(filename,&nsamples,&nchannels,&sampling_rate);
	double *data = read_raw(filename,&nsamples,&nchannels,&sampling_rate,compression);	
	assert(sampling_rate == SAMPLING_RATE);
	assert(nchannels > channel);

	double *waveform = salloc(nsamples*sizeof(double));
	double (*channels)[nchannels] = (void *)data;
	for (index_t i=0; i<nsamples; i++)
	{
		waveform[i] = channels[i][channel];
	}
	free(data);

	*output = waveform;
	return nsamples;
}

/*======================================================================================*/

/* search the clicktrack for a click.  we search either left or right depending
 * on the relation of end to position */

index_t find_click(double *waveform, index_t position, index_t end, double threshold)
{
	/* we want the sample which crosses zero after we have found a peak */
	int found_peak = 0;
	index_t i = position;
	int step = end > position ? 1 : -1;
	dprintf(15,"find_click: from %u to %u step=%d\n",position,end,step);

	while (i != end)
	{
		double val = waveform[i];
//		dp(1, "i=%d\n", (int)i);
		if (((-step)*val < 0) && (found_peak)) 
			return i;
		if ((-step)*val > threshold)
			found_peak=1;
		i += step;
	}
	dprintf(10,"warning: Didn't find the click for position %u!\n",position);
	if ((end > position) && (end - position < 1.5*SAMPLING_RATE))
		dprintf(10,"warning: But it's okay because we were near the end of the file\n");
	if ((end < position) && (position - end < 1.5*SAMPLING_RATE))
		dprintf(10,"warning: But it's okay because we were near the start of the file\n");
	return end;
}

/* generate the clicktrack for the file and write it to disk */

void generate_clicktrack(datafile_t *file)
{
	dprintf(1,"Clicktrack file wasn't found so generating a new one...");

	int oldverbosity=verbosity; verbosity = 0;

	/* we read in the zero'th channel of the file and filter it to get the clicktrack */
	char path[MAX_PATH_LEN];
	int compression = sprint_filepath(path,file);
	double *waveform;
	file->nsamples = read_waveform(&waveform,path,0,0,2*60*SAMPLING_RATE,compression);
	file->clicktrack = filter(waveform,file->nsamples,0,125);
	free(waveform);

	/* now write the clicktrack to file */
	char *ext   = compress_clicktracks ?   ".wv" : ".wav";
	compression = compress_clicktracks ? WAVPACK : UNCOMPRESSED;
	sprintf(path,"%s/%s/%s/clicktracks/clicktrack__%s%s",base_dir,station_dir[file->station],date_dir,file->base_filename,ext);
	sprintf(path,"/tmp/clicktracks/clicktrack__%s%s",file->base_filename,ext);
	write_waveform(path,file->clicktrack,file->nsamples,compression);

	verbosity = oldverbosity;
	dprintf(1,"done\n");
}

/* read in the click track for the file, if it hasn't already been read in.
 * moreover, we may need to generate it, if it doesn't exist. */
void read_clicktrack(datafile_t *file)
{
	if (file->clicktrack != NULL)
		return;

	char path[MAX_PATH_LEN];
	sprintf(path,"%s/%s/%s/clicktracks/clicktrack__%s.wav",base_dir,station_dir[file->station],date_dir,file->base_filename);
	sprintf(path,"/tmp/clicktracks/clicktrack__%s.wav",file->base_filename);
	int compression = UNCOMPRESSED;
	FILE *fp = fopen(path,"r");
	if (!fp)
	{
		// see if a compressed file exists instead
		sprintf(path,"%s/%s/%s/clicktracks/clicktrack__%s.wv",base_dir,station_dir[file->station],date_dir,file->base_filename);
		sprintf(path,"/tmp/clicktracks/clicktrack__%s.wv",file->base_filename);
		fp = fopen(path,"r");
		compression = WAVPACK;
	}
	if (!fp)
	{
		// neither file exists, so generate it
		generate_clicktrack(file);
	}
	else
	{
		// one of the files exists.
		fclose(fp);
		file->nsamples = read_whole_waveform(&file->clicktrack,path,0,compression);
	}
}

/* side specifies in which direction the click is, and sgn specifies the sign of the
 * returned samples since index_t is unsigned. it may seem hacky, but well... it works. */

index_t find_nearest_second(int station, int fileidx, index_t position, int *sgn, int *side)
{
	datafile_t *file = &files[station][fileidx];

	double threshold = click_threshold;	

	read_clicktrack(file);
	/* search to the right for a click */
	index_t clickpos = find_click(file->clicktrack, position, file->nsamples, threshold);
	if (clickpos == file->nsamples)
	{
		// didn't find a click in this direction
		// need to look at the next file
		if (fileidx+1 == num_files[station])
		{
			// we are at last file
			// no more to look at
			dprintf(1,"warning: You're looking at a region right at the end of available data and\n");
		        dprintf(1,"warning: timing may be inaccurate.\n");
			clickpos = position + 2*SAMPLING_RATE;
		}
		else
		{
			datafile_t *nextfile = &files[station][fileidx+1];
			read_clicktrack(nextfile);
			clickpos = find_click(nextfile->clicktrack, 0, nextfile->nsamples,threshold);
			clickpos += file->nsamples;			
		}
	}
	index_t clickpos_distance = clickpos - position;

	/* search to the left for a click */
	index_t clickneg = find_click(file->clicktrack, position, 0, threshold);
	int clickneg_inprevious = 0;
	index_t clickneg_distance = position - clickneg;
	if (clickneg == 0)
	{
		// didn't find click
		if (fileidx == 0)
		{
			dprintf(1,"warning: You're looking at a region right at the end of available data and\n");
		        dprintf(1,"warning: timing may be inaccurate.\n");
			// just take it to be the click we found to the right
			clickneg = clickpos;
			clickneg_distance = clickpos_distance;
		}
		else
		{
			datafile_t *prevfile = &files[station][fileidx-1];
			read_clicktrack(prevfile);
			clickneg = find_click(prevfile->clicktrack, prevfile->nsamples-1,0,threshold);
			clickneg_inprevious = 1;
			clickneg_distance = position + prevfile->nsamples - clickneg;
		}
	}
	

	if (clickneg_distance < clickpos_distance)
	{
		*sgn = (clickneg_inprevious) ? -1 : 1;
		*side = -1;
		return clickneg;
	}
	else
	{
		*sgn = 1;
		*side = 1;
		return clickpos;
	}
}

/* returns the file and the sample position in that file for a specified time */
index_t find_position(int station, double time_in_seconds, int *fileidx_)
{
//	int fileidx = binary_search(files[station],num_files[station],sizeof(datafile_t),&time_in_seconds,file_consider);
	int fileidx;
	for (fileidx = 0; fileidx < num_files[station];fileidx++) {
		if (files[station][fileidx].time_at_eof > time_in_seconds)
			break;
	}
	
	if (fileidx == num_files[station] )
		die("Failed to find a file suitable for time %.3lf seconds for station %d",time_in_seconds,station);
	if (files[station][fileidx].time_at_eof - time_in_seconds > 60.1)
		die("Failed to find a file suitable for time %.3lf seconds for station %d (#1)",time_in_seconds,station);
	*fileidx_ = fileidx;
	datafile_t *file = &files[station][fileidx];
	dprintf(6,"Time of %.3lf (for station %d) should be in %s\n",time_in_seconds,station,file->base_filename);
	read_clicktrack(file);

	// TODO: fileidx == 0
	//double time_at_sof = files[station][fileidx-1].time_at_eof;
	//double time_at_eof = files[station][fileidx].time_at_eof;
	double time_at_eof = files[station][fileidx].time_at_eof;
	// FIXME - use actual number of samples in file instead of 59.9
	double time_at_sof = fileidx>0 ? files[station][fileidx-1].time_at_eof : time_at_eof - 59.904*SAMPLING_RATE;

	// find two approximate positions for the point in the file, one by jumping from the front,
	// the other from jumping from the back
	index_t approx_pos1 = MAX(0, time_in_seconds - time_at_sof)*SAMPLING_RATE;
	index_t approx_pos2 = file->nsamples - MAX(0,time_at_eof - time_in_seconds)*SAMPLING_RATE;

	dprintf(15,"Approx1 = %u samples (%fs)\nApprox2 = %u samples\n",approx_pos1, (time_in_seconds - time_at_sof),approx_pos2);

	int sgn, side;
	index_t click1 = find_nearest_second(station, fileidx, approx_pos1, &sgn, &side);
	index_t click2 = find_nearest_second(station, fileidx, approx_pos2, &sgn, &side);
	dprintf(15,"Click1 = %u Click2 = %u sgn=%d side=%d\n",click1, click2,sgn,side);

	//TODO, recheck this all in the case where sgn is negative
	
	index_t click = click1;
	dp(23, "click = %u\n",click);
	//assert(click1==click2);
	if (click1!=click2)
	{
		// it is possible for this to legitimately happen
		//  case1: the position is right at a second, the left and
		//  right clicks will be two seconds apart
		//  case2: the approx positions are right in the middle of the
		//  second so that one approx may go left and one may go right
		//  (shouldn't happen if the signals are accurate)
		if (labs( (long)click1 - (long)click2 ) > 1.5 * SAMPLING_RATE)
		{
			// they were right on a click
			click = (approx_pos1 + approx_pos2)/2;
			side = 1; sgn = 1;
			dprintf(1,"The point is right on a click, taking the click to be %u\n",click);
		}
		else
		{
			index_t average = (approx_pos1 + approx_pos2)/2;
			click = find_nearest_second(station,fileidx,average,&sgn,&side);
			dprintf(1,"The point is in the middle of a second, taking the click to be %u\n",click);
		}
	}
	else
	{
		dp(23, "clicks were equal\n");
	}


	int64_t position;
	if (side > 0)
	{
		dp(23, "RHS = %lf\n", SAMPLING_RATE * ( ceil(time_in_seconds) - time_in_seconds ));
		if (SAMPLING_RATE * ( ceil(time_in_seconds) - time_in_seconds) > click)
		{
			*fileidx_ = *fileidx_ - 1;
			position = files[station][*fileidx_].nsamples + click - SAMPLING_RATE * ( ceil(time_in_seconds) - time_in_seconds);
		}
		else
		{
			position = click - SAMPLING_RATE * ( ceil(time_in_seconds) - time_in_seconds );
		}
	}
	else
	{
		position = SAMPLING_RATE * ( time_in_seconds - floor(time_in_seconds) ) + sgn*click;
	}
	dp(23, "click = %u\n",click);
	dp(23, "sign = %d\n",sgn);
	dp(23, "pos = %d\n",(int)position);
	dp(23, "timein seconds = %lf\n",time_in_seconds);
	
	return MAX(0,position);
}

index_t determine_nsamples(char *filename, int compression)
{
	index_t nsamples;
	if (compression == UNCOMPRESSED)
	{
		SF_INFO		infile_info = {0};
		SNDFILE		*infile = sf_open(filename, SFM_READ, &infile_info);
		if (!infile) sdie(NULL, "can not open input file %s: ", filename) ;
		nsamples = infile_info.frames;
		sf_close(infile);
	}
	else if (compression == WAVPACK)
	{
		char error[128]; // must be at least 80 chars
		WavpackContext *wavpack = WavpackOpenFileInput(filename,error,OPEN_NORMALIZE,0);
		nsamples                = WavpackGetNumSamples(wavpack);
		WavpackCloseFile(wavpack);
	} else 
		die("determine_nsamples unknown compression type");
	return nsamples;
}

/* grab all the channels for one of the stations */
void grab_station_waveforms(double *waveforms[], int station, double start_in_seconds, double length_in_seconds)
{
	char path[MAX_PATH_LEN];

	double end_in_seconds = start_in_seconds+length_in_seconds;
	index_t length_in_samples = length_in_seconds*SAMPLING_RATE;

	int startidx, endidx;
	index_t start_in_samples, end_in_samples;

	start_in_samples = find_position(station,start_in_seconds, &startidx);
	end_in_samples   = find_position(station,end_in_seconds,   &endidx);
	dprintf(23, "Start is %u\n",start_in_samples);
	dprintf(23, "End is %u\n",end_in_samples);

	int numfiles = endidx - startidx + 1;
	if (numfiles == 1)
	{
		// the whole clip is in a single file
		int compression = sprint_filepath(path,&files[station][startidx]);
		read_all_channels(waveforms,path,start_in_samples,length_in_samples,compression);
		return;
	}

	/* this isn't nice code, but well... doing this isn't nice */
	index_t nsamples;
	index_t total_len=0;

	/* TODO: broken for now */
	int compression = sprint_filepath(path,&files[station][startidx]);
	nsamples = determine_nsamples(path,compression);
	total_len = nsamples - start_in_samples;
	dprintf(23, "start %u -> end %u (+1)\n",start_in_samples,end_in_samples);
	for (int i=1; i<numfiles-1; i++)
	{
		//sprintf(path,"%s/%s",dir,files[station][startidx+i].base_filename);
		compression = sprint_filepath(path,&files[station][startidx+i]);
		nsamples = determine_nsamples(path,compression);
		total_len += nsamples;
	}
	total_len += end_in_samples;
	dprintf(23, "total_len = %u\n",total_len);
	
	// TODO: i haven't tested that it works for numfiles > 2
	for (int i=0; i< NUM_CHANNELS; i++)
	{
		waveforms[i] = salloc(total_len*sizeof(double));
	}
	//printf("okay\n");

	double *temp[NUM_CHANNELS];
	index_t current = 0;
	for (int j=0; j<numfiles; j++)
	{
		int compression = sprint_filepath(path,&files[station][startidx+j]);
		index_t start = 0;
		index_t end   = 2*59.9*SAMPLING_RATE;
		if (j==0)
		{
			start = start_in_samples;
		}
		if (j==numfiles-1)
		{
			end = end_in_samples;
		}

		nsamples = read_all_channels(temp,path,start,end,compression);
		//printf("nsamples for j=%d is %u\n",j,nsamples);
		for (int i=0; i< NUM_CHANNELS; i++)
		{	
		//printf("current = %u\n",current);
			memcpy(waveforms[i]+current,temp[i],nsamples*sizeof(double));
			free(temp[i]);
		}
		current+=nsamples;
	}
	//printf("Total len = %u\n",total_len);
	//printf("zam\n");
}

