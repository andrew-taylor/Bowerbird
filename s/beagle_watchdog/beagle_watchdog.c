#include "i.h"

static char *avr_prefix;
static char *tty_pathname;
static char *feed_string;

static void
do_reboot(void) {
	FILE *f = fopen(tty_pathname, "w");
	if (f)
		fprintf(f, "%s%s\n",  avr_prefix, param_get_string("beagle_watchdog", "reboot"));
	else
		dp(1, "watchdog failed to write to %s\n", tty_pathname);
	dp(1, "waiting for watchdog\n");
	uint32_t feed_seconds = param_get_integer("beagle_watchdog", "feed_seconds");
	sleep(feed_seconds);
	dp(1, "watchdog failed\n");
	reboot(RB_AUTOBOOT);
	die("reboot");
	exit(1); //unreached
}

static void
feed_watchdog(void) {
	FILE *f = fopen(tty_pathname, "w");
	if (f)
		fprintf(f, "%s%s\n",  avr_prefix,  feed_string);
	else
		dp(1, "watchdog failed to write to %s\n", tty_pathname);
}

static void
watchdog(void) {
	avr_prefix = param_get_string_with_default("beagle_watchdog", "prefix", "avr://");
	tty_pathname = param_get_string_with_default("beagle_watchdog", "tty", "/dev/ttyS2");
	feed_string = param_get_string_with_default("beagle_watchdog", "feed", "watchdog pulse");
	char *files_string = sstrdup(param_get_string_with_default("beagle_watchdog", "files", "/var/lib/bowerbird/status/network_up:100000"));
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
	uint32_t feed_seconds = param_get_integer_with_default("beagle_watchdog", "feed_seconds", 300);
	uint32_t granularity_seconds = param_get_integer_with_default("beagle_watchdog", "granularity_seconds", 60);
	uint32_t startup_seconds = param_get_integer_with_default("beagle_watchdog", "startup_seconds", 7200);
//	sleep( get_option_int("wait_seconds"));
	feed_watchdog();
 	for (int i = 0; i < startup_seconds; i += feed_seconds) {
		dp(33, "i=%d\n", i);
		feed_watchdog();
		sleep(feed_seconds);
 	}
	time_t file_last_modified[n_files];	
	time_t now = time(NULL) ;
	for (int f = 0; f < n_files; f++)
		file_last_modified[f] = now;
	while (1) {
 	    for (int i = granularity_seconds/feed_seconds; i >= 0; i--) {
     		dp(33, "i=%d\n", i);
			feed_watchdog();
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

int main(int argc, char *argv[]) 
{
	debug_stream = stderr;
	verbosity = 20; 
	initialize(argc, argv, "beagle_watchdog", "0.1.1", "");
	verbosity = 30;
	
	watchdog();
	return 0;
}


