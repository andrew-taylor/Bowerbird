#include "ts7200_watchdog.h"

#ifdef TS7800
#define SL_ADDR 0x00
#define DATA    (0x04 / sizeof(unsigned int))
#define CONTROL (0x08 / sizeof(unsigned int))
#define STATUS  (0x0C / sizeof(unsigned int))
#define BAUD    (0x0C / sizeof(unsigned int))
#define SRAM    (0x04 / sizeof(unsigned int))
#define GLED    (0x08 / sizeof(unsigned int))

#define READ    1
#define WRITE   0

/* Control Reg */
#define ACK     (1 << 2)
#define IFLG    (1 << 3)
#define STOP    (1 << 4)
#define START   (1 << 5)
#define TWSIEN  (1 << 6)
#define INTEN   (1 << 7)

/* Status Reg

   MT_xxx - master transmitter
   MR_xxx - master receiver
   ST_xxx - slave transmitter
   SR_xxx - slave receiver
*/

#define MT_START        0x08
#define MR_START        0x08
#define MT_REP_START    0x10
#define MT_SLA_ACK      0x18
#define MT_DATA_ACK     0x28
#define MR_SLA_ACK      0x40
#define MR_DATA_ACK     0x50
#define MR_DATA_NACK    0x58
#define SR_SLA_ACK      0x60
#define SR_DATA_ACK     0x80
#define SR_STOP         0xA0
#define ST_SLA_ACK      0xA8
#define ST_DATA_ACK     0xB8

/* Slave Address Reg */
#define GCE             (1 << 0)
#define SADDR           0x0A            //Orion I2C address

#define AVR_ADDR        (0x07 << 1)     //AVR I2C address

/* I2C commands */

#define LED             0x04
#define SLEEP           0x08
#define ADC_CH0         (0x40 | (1<<0))
#define ADC_CH1         (0x40 | (1<<1))
#define ADC_CH2         (0x40 | (1<<2))
#define ADC_CH3         (0x40 | (1<<3))
#define ADC_CH6         (0x40 | (1<<4))
#define ADC_CH7         (0x40 | (1<<5))

#define WDT_OFF         0
#define WDT_250ms       1
#define WDT_2s          2
#define WDT_8s          3

#define OTP_W           0x81
#define OTP_R		0x80

volatile unsigned int *data, *control, *status, *led;
unsigned int verbose = 1, addr;

void static inline twi_write(unsigned char dat) {
	if(verbose) fprintf(stderr,"Writing data (0x%x)\n", dat);
	*data = dat;
	usleep(100);

	*control = (*control & ~IFLG) | ACK;
	usleep(100);
	while((*control & IFLG) == 0x0);        // Wait for an I2C event


	if((*status != ST_DATA_ACK) && (*status != MT_DATA_ACK )) {
		if(verbose) fprintf(stderr,"Slave didn't ACK data\n");
		exit(1);
	}

	if(verbose) fprintf(stderr,"Slave ACKed data\n");
}
	
unsigned char static inline twi_read(void) {


	if(verbose) fprintf(stderr, "Waiting for data from master\n");
	*control = (*control & ~IFLG) | ACK;
	while((*control & IFLG) == 0x0);	// Wait for an I2C event 
	if((*status != SR_DATA_ACK) && (*status != MR_DATA_ACK)) {
		if(verbose) fprintf(stderr, "Error reading data from master(0x%x)\n", *status);
		exit(1);
	
	}

	return (unsigned char)*data;
}

void static inline twi_select(unsigned char addr, unsigned char dir) {
	unsigned int timeout = 0;
	if(verbose) fprintf(stderr,"Attempting to send start signal\n");
	*control = START|TWSIEN;        // Send start signal
	usleep(1);
	while((*control & IFLG) == 0)
		if(timeout++ > 10000) {
			if(verbose) fprintf(stderr, "Timedout\n");
			*control = STOP|TWSIEN; // Send stop signal
			if(verbose) fprintf(stderr,"Sent Stop signal\n");
			exit(-1);
		}
	if((*status != MT_START) && (*status != MT_REP_START)) {
		if(verbose) fprintf(stderr,"Couldn't send start signal(0x%x)\n",
		  *status);
		*control = STOP|TWSIEN; // Send stop signal
		if(verbose) fprintf(stderr,"Sent Stop signal\n");
		exit(-1);
	}

	if(verbose) fprintf(stderr,"Sent start signal succesfully\n"
	  "Attempting to select slave\n");
	*data = addr | dir;     // Send SLA+W/R
	usleep(1);

	*control = (*control & ~IFLG) | ACK;
	usleep(1);

	while((*control & IFLG) == 0) ;

	if((*status != MT_SLA_ACK) && (*status != MR_SLA_ACK)) {
		if(verbose) fprintf(stderr,"Slave didn't ACK select signal(0x%x)\n", *status);
		*control = STOP|TWSIEN; // Send stop signal
		if(verbose) fprintf(stderr,"Sent Stop signal\n");
		exit(-1);
		
	}
	if(verbose) fprintf(stderr,"Succesfully selected slave\n");
}

void ts7800_init(void) {
	dp(10, "ts7800_init\n");
	int devmem = open("/dev/mem", O_RDWR|O_SYNC);
	assert(devmem != -1);
	unsigned int *twi_regs = (unsigned int *)mmap(0, getpagesize(), PROT_READ|PROT_WRITE,
	  MAP_SHARED, devmem, 0xF1011000);

	unsigned int *regs = (unsigned int *)mmap(0, getpagesize(), PROT_READ|PROT_WRITE,
	  MAP_SHARED, devmem,  0xe8000000);
	led = &regs[GLED];
        control = &twi_regs[CONTROL];
        data = &twi_regs[DATA];
        status = &twi_regs[STATUS];
}

void ts7800_feed_watchdog(void);

void ts7800_feed_watchdog(void) {
	dp(10, "ts7800_feed_watchdog\n");
	twi_select(AVR_ADDR, WRITE);
	twi_write(WDT_8s);
	*control = STOP|TWSIEN; // Send stop signal
}
#endif

static void
do_reboot(void) {
	dp(1, "waiting for watchdog\n");
	sleep(30);
	dp(1, "watchdog failed\n");
	reboot(RB_AUTOBOOT);
	die("reboot");
	exit(1); //unreached
}

static void
watchdog(void) {
	char *files_string = get_option("files");
	int n_files = files_string && strlen(files_string) > 0;
	for (char *p = files_string; p ; p = strchr(p, ',')) {
		n_files++;
		p++;
	}
	char *file_names[n_files];
	uint32_t file_times[n_files];
	n_files = 0;
    for (char *p = files_string; p ;) {
		char *q = strchr(p, ',');
		if (q)
			*q = '\0';
		if (strlen(p)) {
			char *time = strrchr(p, ':');
			if (!time || strlen(time+1) == 0)
				die("Bad watchdog time specification: '%s'", p);
			*time = '\0';
			char *invalid = NULL;
			file_times[n_files] = strtol(time+1, &invalid, 10);
			if (!invalid || *invalid)
				die("Bad watchdog time specification: '%s'", p);
			file_names[n_files] = sstrdup(p);
			dp(1, "file %d '%s' must be modified at least every %d seconds\n", n_files, file_names[n_files], file_times[n_files]);
			n_files++;
		}
		p = q;
		if (p)
			p++;
    }
	uint32_t feed_seconds = get_option_int("feed_seconds");
	uint32_t granularity_seconds = get_option_int("granularity_seconds");
	uint32_t startup_seconds = get_option_int("startup_seconds");
	sleep( get_option_int("wait_seconds"));
#ifdef TS7800
	ts7800_init();
	ts7800_feed_watchdog();
#else
	int fd = open("/dev/mem", O_RDWR|O_SYNC);
	assert(fd != -1);
	volatile uint8_t *watchdog_control = (unsigned char *)mmap(0, getpagesize(), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x23800000);
	volatile uint8_t *watchdog_feed = (unsigned char *)mmap(0, getpagesize(), PROT_READ|PROT_WRITE, MAP_SHARED,fd, 0x23C00000);
	assert(watchdog_control != MAP_FAILED && watchdog_feed != MAP_FAILED);
		
	// write  to the two addresses need to occur within 30us
	*watchdog_feed = 0x05;
	*watchdog_control = 0x07;  // 8 seconds
#endif
 	for (int i = 0; i < startup_seconds; i += feed_seconds) {
		dp(33, "i=%d\n", i);
#ifdef TS7800
	ts7800_feed_watchdog();
#else
		*watchdog_feed = 0x05;
#endif
		sleep(feed_seconds);
 	}
// 	if (!n_files) {
// 		dp(1, "No files to watch so exiting\n");
//		exit(0);
// 	}
	time_t file_last_modified[n_files];	
	time_t now = time(NULL) ;
	for (int f = 0; f < n_files; f++)
		file_last_modified[f] = now;
	while (1) {
 	    for (int i = granularity_seconds/feed_seconds; i >= 0; i--) {
     		dp(33, "i=%d\n", i);
#ifdef TS7800
			ts7800_feed_watchdog();
#else
			*watchdog_feed = 0x05;
#endif
   			sleep(feed_seconds);
    	}
		for (int f = 0; f < n_files; f++) {
			if (!file_names[f])
				continue;
			struct stat buf;
			dp(33, "stat(%s)\n", file_names[f]);
			if (stat(file_names[f], &buf) == 0)
				file_last_modified[f] = buf.st_mtime;
			uint32_t age = time(NULL) - file_last_modified[f];
			if (age > file_times[f]) {
				dp(1, "Rebooting because file %d '%s' has not been modified for %d seconds\n", n_files, file_names[f], age);
				do_reboot();
			}
		}
    }
}

static void usage(void) {
	fprintf(stderr, "Usage %s [-V] [-v verbosity] <commands> \n", myname);
}

char *version = "0.1";

static char *short_options = "o:v:V";
static struct option long_options[] = {
	{"option", 1, 0, 'o'},
	{"verbosity", 1, 0, 'v'},
	{"version", 0, 0, 'V'},
	{0, 0, 0, 0}
};

int main(int argc, char *argv[]) {
	verbosity = 1;
	debug_stream = stderr;
	set_myname(argv);
	optind = 0;
	initialize_options();
	while (1) {
		int option_index;
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (c == -1)
			break;
		opterr = 0;
		switch (c) {
		case 'o':
			parse_option_assignment(optarg);
 			break;
		case 'v':
			verbosity = atoi(optarg);
  			break;
		case 'V':
			printf("%s v%s\n",myname, version);
  			exit(0);
		case '?':
			usage();
 		}
	}
	close(0);
	close(1);
	setsid();
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	for (int i = 1; i <= 32; i++)
		signal(i, SIG_IGN);
	watchdog();
	return 0;
}

void
initialize_options(void) {
	set_option_int("feed_seconds", 4);
	set_option_int("granularity_seconds", 3600);
	set_option("files", "");
	set_option_int("wait_seconds",10); // wait this number of seconds before setting the watchdog 
	set_option_int("startup_seconds", 3600); // feed watchdog for this period of time before monitoring files
}
