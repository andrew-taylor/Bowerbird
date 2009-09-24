#include <gsl/gsl_fit.h>
#ifdef USE_FFTW
#include <fftw3.h>
#endif
#ifdef USE_KISS_FFT
#include "kiss_fft.h"
#include "kiss_fftr.h"
#endif

#include "bowerbird.h"
#include "spectral_analysis-prototypes.h"

#define SPECTRAL_ANALYSIS_GROUP "spectral_analysis"
