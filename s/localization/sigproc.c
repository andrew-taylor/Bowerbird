/* signal processing functions.  my own filtering functions have been replaced
 * by calls to the external sox program. :(  */

#include "i.h"

double* filter(double *in, index_t nsamples, double lowfreq, double highfreq)
{
	write_waveform("/tmp/nyinput.wav",in,nsamples,UNCOMPRESSED);
	char line[MAX_PATH_LEN];
	sprintf(line,"sox /tmp/nyinput.wav /tmp/nyoutput.wav filter %.0lf-%.0lf",lowfreq,highfreq);
	if (system(line) == -1) die("Filtering failed. Exiting...\n");
	double *filtered;
	index_t nsamples2 = read_whole_waveform(&filtered,"/tmp/nyoutput.wav",0,UNCOMPRESSED);
	assert(nsamples == nsamples2);
	return filtered;
}

double* bandpass_filter(double *in, index_t nsamples, double centre, double width)
{
	write_waveform("/tmp/nyinput.wav",in,nsamples,UNCOMPRESSED);
	char line[MAX_PATH_LEN];
	sprintf(line,"sox /tmp/nyinput.wav /tmp/nyoutput.wav band %lf %lfh",centre,width);
	if (system(line) == -1) die("Filtering failed. Exiting...\n");
	double *filtered;
	index_t nsamples2 = read_whole_waveform(&filtered,"/tmp/nyoutput.wav",0,UNCOMPRESSED);
	assert(nsamples == nsamples2);
	return filtered;
}

double* highpass_filter(double *in, index_t nsamples, double lowfreq)
{
	write_waveform("/tmp/nyinput.wav",in,nsamples,UNCOMPRESSED);
	char line[MAX_PATH_LEN];
	sprintf(line,"sox /tmp/nyinput.wav /tmp/nyoutput.wav highpass %lf",lowfreq);
	if (system(line) == -1) die("Filtering failed. Exiting...\n");
	double *filtered;
	index_t nsamples2 = read_whole_waveform(&filtered,"/tmp/nyoutput.wav",0,UNCOMPRESSED);
	assert(nsamples == nsamples2);
	return filtered;
}

/* this was just used when testing some of the filters. inefficiently and badly coded.
 * it plots the powerspectrum of the input waveform */
void plotpowerspectrum(double *inorig, index_t nsamples,int fft_size)
{
	int num_ffts = nsamples/fft_size;
	double *in = inorig;
	if (nsamples % fft_size != 0)
	{
		dp(15,"plotpowerspectrum: Losing %d samples at end (out of %d)\n",nsamples - fft_size*num_ffts,nsamples);
	}
	
	index_t outsize = fft_size/2+1;

	double *frequencies = (double *)salloc(sizeof(double)*outsize);
	for (index_t i = 0; i<outsize; i++)
	{
		frequencies[i] = i*SAMPLING_RATE/fft_size;
	}

	fftw_complex *out;
	out = (fftw_complex *)salloc(sizeof(fftw_complex)*outsize);
	double *outabs = (double *)salloc(sizeof(double)*outsize);
	double *finaloutput = (double *)salloc(sizeof(double)*outsize);
	memset(finaloutput,0,sizeof(double)*outsize);

	for (int i=0; i<num_ffts; i++)
	{
		fftw_plan plan = fftw_plan_dft_r2c_1d(fft_size,in+i*fft_size,out,FFTW_ESTIMATE);
		fftw_execute(plan);
		fftw_destroy_plan(plan);

		for (index_t k=0; k<outsize; k++)
		{
			out[k] = cabs(out[k])/outsize;
			out[k] = 10*log10(out[k]);
		}

		// now for averaging
		int radius = 16;
		for (index_t k=0; k<outsize; k++)
		{ 
			if ((k >=radius) && (k+radius < outsize))
			{
				outabs[k] = 0;
				for (int j=-radius; j<=radius; j++)
					outabs[k] += out[k+j];
				outabs[k] = outabs[k]/(2*radius+1);
			}
			else if (k+radius < outsize)
			{
				outabs[k] = 0;
				for (int j=-k; j<=radius; j++)
					outabs[k] += out[k+j];
				outabs[k] = outabs[k]/(radius+k+1);
			}
			else if (k >= radius)
			{
				outabs[k] = 0;
				for (int j=-radius; k+j<outsize; j++)
					outabs[k] += out[k+j];
				outabs[k] = outabs[k]/(radius+(outsize-k));
			}

			finaloutput[k] += outabs[k]/num_ffts;
		}

	}
	plotrealx(frequencies,finaloutput,outsize);

	free(outabs);
	free(out);
	free(frequencies);
	free(finaloutput);
}

double *lowpass_old(double *in_,index_t nsamples, double lowfreq, int fft_size)
{
	int num_ffts = nsamples/fft_size;
	double *in = in_;
	if (nsamples % fft_size != 0)
	{
		num_ffts++;
		in = (double *)salloc(sizeof(double)*num_ffts*fft_size);
		memcpy(in,in_,sizeof(double)*nsamples);
		memcpy(in+nsamples,in_,sizeof(double)*(num_ffts*fft_size-nsamples));
	}

	double *filtered = salloc(sizeof(double)*nsamples);

	index_t outsize = fft_size/2+1;
	fftw_complex *out;
	out = (fftw_complex *)salloc(sizeof(fftw_complex)*outsize);

	double *killwindow = (double *)salloc(sizeof(double)*outsize);
	for (index_t j=0; j<outsize; j++)
	{
		double freq = j*SAMPLING_RATE/fft_size;
		killwindow[j]=1.0;
		if (freq > lowfreq)
			killwindow[j] = 0.0;
	}

	for (int i=0; i<num_ffts; i++)
	{
		fftw_plan plan = fftw_plan_dft_r2c_1d(fft_size,in+i*fft_size,out,FFTW_ESTIMATE);
		fftw_execute(plan);
		fftw_destroy_plan(plan);

		for (index_t j=0; j<outsize; j++)
		{
			out[j] = out[j]*killwindow[j];
		}

		if (i != num_ffts-1)
		{
			plan = fftw_plan_dft_c2r_1d(fft_size,out,filtered+i*fft_size,FFTW_ESTIMATE);
			fftw_execute(plan);
			fftw_destroy_plan(plan);
		}
		else
		{
			double *result = salloc(sizeof(double)*fft_size);
			plan = fftw_plan_dft_c2r_1d(fft_size,out,result,FFTW_ESTIMATE);
			fftw_execute(plan);
			fftw_destroy_plan(plan);
			memcpy(filtered+i*fft_size,result,(nsamples - fft_size*(num_ffts-1))*sizeof(double));
			free(result);
		}
	}

	for (index_t j=0; j<nsamples; j++)
	{
		filtered[j] = filtered[j] / fft_size;
	}

	free(killwindow);
	free(out);
	if (nsamples % fft_size != 0)
	{
		free(in);
	}

	return filtered;
}
