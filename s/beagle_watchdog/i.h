#ifndef BEAGLE_WATCHDOG_H
#define BEAGLE_WATCHDOG_H
#include <stdbool.h>
#include <float.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/reboot.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "bowerbird.h"
#include "beagle_watchdog-prototypes.h"

#endif
