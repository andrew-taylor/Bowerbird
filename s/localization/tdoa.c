#include "i.h"

/* the crosspower spectrum is the fourier transform of the cross-correlation.
 * some equations: http://mathworld.wolfram.com/Cross-Correlation.html
 * so we just reverse one of the waveforms, take their fourier transforms,
 * and then multiply these fourier transforms together pointwise.  */
fftw_complex *crosspower_spectrum(double *waveform1, double *waveform2, index_t nsamples)
{
	double *waveform2reversed = (double *)salloc(sizeof(double)*nsamples);
	for (index_t i=0,j=nsamples-1; i<nsamples; i++,j--) {
		waveform2reversed[i] = waveform2[j];
	}
	
	
	fftw_complex *out1, *out2;
	out1 = (fftw_complex *) salloc(sizeof(fftw_complex) * nsamples);
	out2 = (fftw_complex *) salloc(sizeof(fftw_complex) * nsamples);

	// take the DFT of both channels
	fftw_plan plan1, plan2;
	plan1 = fftw_plan_dft_r2c_1d(nsamples,waveform1,out1,FFTW_ESTIMATE);
	plan2 = fftw_plan_dft_r2c_1d(nsamples,waveform2reversed,out2,FFTW_ESTIMATE);
	fftw_execute(plan1);
	fftw_execute(plan2);
	fftw_destroy_plan(plan1);
	fftw_destroy_plan(plan2);

	// now multiply them together (result in out1)
	for (index_t i=0; i<nsamples; i++) {
		out1[i] = out1[i] * out2[i];
	}

	free(out2);
	free(waveform2reversed);
	return out1;
}

/* returns the estimated TDOA (in seconds) between the two waveforms.
 * Our method uses the generalized cross-correlation (GCC) between the
 * two waveforms.  GCC is a venerable method for estimating
 * time-difference-of-arrival.  In fact, it is really a collection of
 * methods which can all be viewed as weightings of the cross-correlation
 * taken in the spectral domain:
 *
 *	C.H. Knapp and - The Generalized Correlation Method for
 *	G. C. Carter     Estimation of Time Delay,
 *			 IEEE Transactions on Acoustics, Speech,
 *			 and Signal Processing, vol. 24, no. 4
 *			 August 1976, pp. 320-327
 *
 * The various different weightings (e.g. SCOT, ROTH, PHAT) each have
 * a different theoretical basis (e.g. the Maximum Likelihood estimator
 * under certain signal models), and their practical performance has 
 * been well-studied over the years.  From reading other papers it
 * seems widely accepted that PHAT achieves good performance in noisy
 * environments.  The other weightings seem to have lost favour.
 *
 * Strangely, PHAT doesn't work very well for us, and we are just using
 * the plain cross-correlation method (no weighting).  Perhaps I have
 * implemented PHAT incorrectly. It may have something to do with the
 * fact that the DFT implemented by fftw3 is unnormalized, which might
 * be messing up the weighting in the frequency domain. Just a guess.
 *
 * For amusement purposes, notice how the second author's initials are
 * GCC, which is the commonly used acronym for referring to Generalized
 * Cross-Correlation.
 */
double time_difference_of_arrival(double *waveform1,double *waveform2,index_t nsamples,int method,double *heuristic)
{
	if (graphing >= 5)
	{
		gnuplotf("rows=2 length=%d title='Input Waveforms' %lf ; length=%d title='bb' %lf",nsamples,waveform1,nsamples,waveform2);
	}

	dprintf(6,"Computing cross-correlation...");
	fftw_complex *crosspower = crosspower_spectrum(waveform1,waveform2,nsamples);

	switch (method)
	{
		case GCC_PLAIN:
			break;
		case GCC_PHAT:
			for (index_t i=0; i<nsamples; i++) {
				crosspower[i] = crosspower[i] / cabs(crosspower[i]); 
			}
			break;
		case GCC_ROTH:
			{
				fftw_complex* G11 = crosspower_spectrum(waveform1,waveform1,nsamples);
				for (index_t i=0; i<nsamples; i++) {
					crosspower[i] = crosspower[i] / G11[i];
				}
				free(G11);
			}
			break;
		case GCC_ROTH2:
			{
				fftw_complex* G22 = crosspower_spectrum(waveform2,waveform2,nsamples);
				for (index_t i=0; i<nsamples; i++) {
					crosspower[i] = crosspower[i] / G22[i];
				}
				free(G22);
			}
			break;
		case GCC_SCOT:
			{
				fftw_complex* G11 = crosspower_spectrum(waveform1,waveform1,nsamples);
				fftw_complex* G22 = crosspower_spectrum(waveform2,waveform2,nsamples);
				for (index_t i=0; i<nsamples; i++) {
					crosspower[i] = crosspower[i] / csqrt(G11[i]*G22[i]);
				}
				free(G11);
				free(G22);
			}
			break;
		case GCC_ML:
			{
				fftw_complex* G11 = crosspower_spectrum(waveform1,waveform1,nsamples);
				fftw_complex* G22 = crosspower_spectrum(waveform2,waveform2,nsamples);
				for (index_t i=0; i<nsamples; i++) {
					double g12 = crosspower[i]/csqrt(G11[i]*G22[i]);
					double g12sqr = square_d(cabs(g12));
					crosspower[i] = (crosspower[i]*g12sqr)/((1-g12sqr)*cabs(crosspower[i]));
				}
				free(G11);
				free(G22);
			}
			break;
	}

	/* take inverse FFT */
	double *result = (double *)salloc(sizeof(double)*nsamples);
	fftw_plan plan = fftw_plan_dft_c2r_1d(nsamples, crosspower, result, FFTW_ESTIMATE);
	fftw_execute(plan);
	fftw_destroy_plan(plan);
	free(crosspower);
	
	
	// the FFT we're using is unnormalized
	for (index_t i=0; i<nsamples; i++) {
		result[i] = result[i]/nsamples;
		result[i] = fabs(result[i]);
	}
	dprintf(6,"done\n");

	if (graphing >= 5)
	{
		switch(method)
		{
			case GCC_PLAIN:
				gnuplotf("length=%d title='Cross-Correlation using plain method' %lf",nsamples,result);
				break;
			case GCC_PHAT:
				gnuplotf("length=%d title='Cross-Correlation using PHAT method' %lf",nsamples,result);
				break;
			case GCC_ROTH:
				gnuplotf("length=%d title='Cross-Correlation using ROTH method' %lf",nsamples,result);
				break;
			case GCC_ROTH2:
				gnuplotf("length=%d title='Cross-Correlation using ROTH2 method' %lf",nsamples,result);
				break;
			case GCC_SCOT:
				gnuplotf("length=%d title='Cross-Correlation using SCOT method' %lf",nsamples,result);
				break;
			case GCC_ML:
				gnuplotf("length=%d title='Cross-Correlation using ML method' %lf",nsamples,result);
				break;
		}
	}

	double max;
	int delay;
	max = maxwithindex(result,nsamples,(unsigned int *)&delay);
	if (delay >= nsamples/2-1) //one off?
		delay = delay-nsamples; 


	dprintf(10,"\tMaximum correlation of %lf at delay %d\n",max,delay);

	if (graphing >= 5)
	{
		index_t len; 
		if (delay < 0)
		{
			len = nsamples + delay;
			plot2real(waveform1,len,waveform2-delay,len);
		}
		else
		{
			len = nsamples - delay;
			plot2real(waveform1+delay,len,waveform2,len);
		}
	}

	// some heuristics to get an idea for how good this is 
	double badness = 0;
	double threshold = 0.65*max;
	for (index_t i=0; i<nsamples; i++)
	{
		if (result[i] > threshold)
		{
			int position = i;
			if (i >= nsamples/2-1)
				position = i - nsamples;
			double dist = fabs(position - delay);
			badness += square_d(dist);
/*			if (dist >= 2000)
				badness += square_d(dist-2000);*/
		}
	}
	badness /= nsamples;
	heuristic[TDOA_BADNESS_HEUR] = badness;
/*
	int up = 0;
	double current = result[0];
	index_t peaks=0;
	count=0;
	threshold = 0.5*max;
	for (index_t i = 1; i<nsamples; i++)
	{
		if (result[i] > current)
		{
			up=1;
		}

		if (result[i] < current)
		{
			if (up)
			{
				peaks++;
				up=0;
				if (result[i-1] > threshold)
					count++;
			}
		}
	}
	dprintf(5,"Peaks = %d\n",peaks);
	dprintf(5,"Above threshold = %d\n",count);
	dprintf(5,"Percentage above = %lf\n",(100.0*count)/nsamples);
	*heuristic = (100.0*count)/nsamples;
*/	
	double tdoa = ((double)delay)/SAMPLING_RATE;

	heuristic[TDOA_TDOA_HEUR] = tdoa;


	free(result);
	return tdoa;
}
