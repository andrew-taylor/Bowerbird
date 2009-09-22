#include <alsa/asoundlib.h>
#undef assert
#include "i.h"

static snd_pcm_t *pcm_handle;

static char *pcm_name;
static int rate;
static int n_periods;
static unsigned long period_size;
static int n_channels;
static unsigned long desired_buffer_size;


void
alsa_close(void) {         
	snd_pcm_close(pcm_handle);
}

int 
alsa_readi(int16_t *data, int n_frames) {
    dp(30, "n_frames=%d\n", n_frames);
    for(int j = 0;;++j) {
		dp(30, "loop %d\n", j);
	    for (int i = 0; i < 10; i++) {
			int rc = snd_pcm_readi(pcm_handle, data, n_frames);
			if (rc > 0) {
				dp(30, "pcm read %d frames\n", rc);
				return rc;
			} else if (rc == -EPIPE) {
				/* EPIPE means overrun */
	    		dp(0, "overrun occurred\n");
				snd_pcm_prepare(pcm_handle);
			} else if (rc < 0) {
	    		dp(0, "error from read: %s\n", snd_strerror(rc));
	    		break;
	    	}
	    }
	    sleep(1);
		snd_pcm_close(pcm_handle);
		alsa_init();
	}
}

/** \brief Initialise alsa device 
 * \param pcm_name Name of the PCM device, like plughw:0,0. 
 * The first number is the number of the soundcard, 
 * the second number is the number of the device.
 * \param rate Sample rate
 * \param n_periods Number of periods (used to be called fragments)
 * \param period_size Size of the periods (bytes) 
 * (Currently ignored in favour of desired_buffer_size)
 * \param n_channels Number of channels
 * \param desired_buffer_size Can't remember.
 *
 * This saves the parameters to static variables,
 * the calls alsa_init and prints a message if it fails.
 */
void    
do_alsa_init(char *p_pcm_name, int p_rate, int p_n_periods, unsigned long p_period_size, int p_n_channels, unsigned long p_desired_buffer_size)
{
    dp(30, "pcm_name = %s\n", p_pcm_name);
	pcm_name = p_pcm_name;
	rate = p_rate;
	n_periods = p_n_periods;
	period_size = p_period_size;
	n_channels = p_n_channels;
	desired_buffer_size = p_desired_buffer_size;
	
	for (int i = 0; i < 1; i++) {
		if (alsa_init() == 0)
			return;
		sleep(1);
	}
	die("do_alsa_init failed");
}

/** \brief Initialise alsa device
 *
 * It really just initilises the device. This should only be called directly 
 * after do_alsa_init has been called. It shouldn't be called by methods outside
 * this file.
 */
int    
alsa_init(void) 
{
	int err;

    /* Playback stream */
    snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;

    /* This structure contains information about    */
    /* the hardware and can be used to specify the  */      
    /* configuration to be used for the PCM stream. */ 
    snd_pcm_hw_params_t *hwparams;

    /* Allocate the snd_pcm_hw_params_t structure on the stack. */
    snd_pcm_hw_params_alloca(&hwparams);


    /* Open PCM. The last parameter of this function is the mode. */
    /* If this is set to 0, the standard mode is used. Possible   */
    /* other values are SND_PCM_NONBLOCK and SND_PCM_ASYNC.       */ 
    /* If SND_PCM_NONBLOCK is used, read / write access to the    */
    /* PCM device will return immediately. If SND_PCM_ASYNC is    */
    /* specified, SIGIO will be emitted whenever a period has     */
    /* been completely processed by the soundcard.                */
    if ((err = snd_pcm_open(&pcm_handle, pcm_name, stream, 0)) < 0) {
      fprintf(stderr, "Error opening PCM device %s. (%s)\n", pcm_name, snd_strerror(err));
      return(-1);
    }
    /* Init hwparams with full configuration space */
    if ((err = snd_pcm_hw_params_any(pcm_handle, hwparams)) < 0) {
      fprintf(stderr, "Can not configure this PCM device. (%s)\n", snd_strerror(err));
      return(-1);
      }
    unsigned int exact_rate;   /* Sample rate returned by */
                      /* snd_pcm_hw_params_set_rate_near */ 
    /* Set access type. This can be either    */
    /* SND_PCM_ACCESS_RW_INTERLEAVED or       */
    /* SND_PCM_ACCESS_RW_NONINTERLEAVED.      */
    /* There are also access types for MMAPed */
    /* access, but this is beyond the scope   */
    /* of this introduction.                  */
    if ((err = snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
      fprintf(stderr, "Error setting access. (%s)\n", snd_strerror(err));
      return(-1);
    }
  
    /* Set sample format */
    if ((err = snd_pcm_hw_params_set_format(pcm_handle, hwparams, SND_PCM_FORMAT_S16_LE)) < 0) {
      fprintf(stderr, "Error setting format. (%s)\n", snd_strerror(err));
      return(-1);
    }

    /* Set sample rate. If the exact rate is not supported */
    /* by the hardware, use nearest possible rate.         */ 
    exact_rate = rate;
    if ((err = snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &exact_rate, 0)) < 0) {
      fprintf(stderr, "Error setting rate. (%s)\n", snd_strerror(err));
      return(-1);
    }
    if (rate != exact_rate) {
      fprintf(stderr, "The rate %d Hz is not supported by your hardware.\n  ==> Using %d Hz instead.\n", rate, exact_rate);
    }

    /* Set number of channels */
    if ((err = snd_pcm_hw_params_set_channels(pcm_handle, hwparams, n_channels)) < 0) {
      fprintf(stderr, "Error setting device to %d channels. (%s)\n", n_channels, snd_strerror(err));
      return(-1);
    }

    /* Set number of periods. Periods used to be called fragments. */ 
    if ((err = snd_pcm_hw_params_set_periods(pcm_handle, hwparams, n_periods, 0)) < 0) {
      fprintf(stderr, "Error setting periods. (%s)\n", snd_strerror(err));
      return(-1);
    }
    /* Set buffer size (in frames). The resulting latency is given by */
    /* latency = period_size * n_periods / (rate * bytes_per_frame)     */
//    if (snd_pcm_hw_params_set_buffer_size(pcm_handle, hwparams, (period_size * n_periods)>>2) < 0) {
   	unsigned long buffer_size = desired_buffer_size;
    if ((err = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hwparams, &buffer_size)) < 0) {
      fprintf(stderr, "Error setting buffersize. (%s)\n", snd_strerror(err));
      return(-1);
    }
	if (desired_buffer_size != buffer_size) {
		fprintf(stderr, "Asked for buffer size %ld, got %ld, should be okay...\n", desired_buffer_size, buffer_size);
	}
    /* Apply HW parameter settings to */
    /* PCM device and prepare device  */
    if ((err = snd_pcm_hw_params(pcm_handle, hwparams)) < 0) {
      fprintf(stderr, "Error setting HW params. (%s)\n", snd_strerror(err));
      return( -1);
    }
    return 0;
}

void
alsa_info(void) {
  int val;

  printf("ALSA library version: %s\n",
          SND_LIB_VERSION_STR);

  printf("\nPCM stream types:\n");
  for (val = 0; val <= SND_PCM_STREAM_LAST; val++)
    printf("  %s\n",
      snd_pcm_stream_name((snd_pcm_stream_t)val));

  printf("\nPCM access types:\n");
  for (val = 0; val <= SND_PCM_ACCESS_LAST; val++)
    printf("  %s\n",
      snd_pcm_access_name((snd_pcm_access_t)val));

  printf("\nPCM formats:\n");
  for (val = 0; val <= SND_PCM_FORMAT_LAST; val++)
    if (snd_pcm_format_name((snd_pcm_format_t)val)
      != NULL)
      printf("  %s (%s)\n",
        snd_pcm_format_name((snd_pcm_format_t)val),
        snd_pcm_format_description(
                           (snd_pcm_format_t)val));

  printf("\nPCM subformats:\n");
  for (val = 0; val <= SND_PCM_SUBFORMAT_LAST;
       val++)
    printf("  %s (%s)\n",
      snd_pcm_subformat_name((
        snd_pcm_subformat_t)val),
      snd_pcm_subformat_description((
        snd_pcm_subformat_t)val));

  printf("\nPCM states:\n");
  for (val = 0; val <= SND_PCM_STATE_LAST; val++)
    printf("  %s\n",
           snd_pcm_state_name((snd_pcm_state_t)val));
}
