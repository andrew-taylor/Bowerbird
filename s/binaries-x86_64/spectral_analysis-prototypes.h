
/* spectral_analysis/kiss_fft.c */

#ifdef _OPENMP
#endif
kiss_fft_cfg kiss_fft_alloc(int nfft,int inverse_fft,void *mem,size_t *lenmem );
void kiss_fft_stride(kiss_fft_cfg st,const kiss_fft_cpx *fin,kiss_fft_cpx *fout,int in_stride);
void kiss_fft(kiss_fft_cfg cfg,const kiss_fft_cpx *fin,kiss_fft_cpx *fout);
void kiss_fft_cleanup(void);
int kiss_fft_next_fast_size(int n);

/* spectral_analysis/kiss_fftr.c */

#ifdef USE_SIMD    
#endif    
kiss_fftr_cfg kiss_fftr_alloc(int nfft,int inverse_fft,void *mem,size_t *lenmem);
void kiss_fftr(kiss_fftr_cfg st,const kiss_fft_scalar *timedata,kiss_fft_cpx *freqdata);
#ifdef USE_SIMD    
#else
#endif
void kiss_fftri(kiss_fftr_cfg st,const kiss_fft_cpx *freqdata,kiss_fft_scalar *timedata);
#ifdef USE_SIMD        
#else
#endif

/* spectral_analysis/power_test.c */

#ifdef SIMPLE_FFT_TEST
void simple_fft(void);
#endif
#ifdef FIXED_POINT
#else
#endif
void test_power_phase(void);
#ifdef DO_IMAGE
#endif

/* spectral_analysis/estimate_sinusoid_parameters_test.c */

#ifdef POWER_T_32
#else
#ifdef POWER_T_DOUBLE
#else
#endif
#endif

/* spectral_analysis/track_sinusoids_test.c */

#ifdef OLD
void compare_sinusoids(sinusoid_t s1, sinusoid_t s2);
#endif
#ifdef OLD
#endif

/* spectral_analysis/peaks_test.c */

int find_peaks_simple(int length, double *restrict data, int *restrict peaks);

/* spectral_analysis/extract_calls.c */


/* spectral_analysis/sound_to_image.c */

void sound_to_image(char *sound_file, char *image_file);

/* spectral_analysis/silence_removal.c */

void silence_removal_file(char *infilename, char *outfilename);

/* spectral_analysis/score_calls.c */

