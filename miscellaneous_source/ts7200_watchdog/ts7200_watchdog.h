#ifndef WATCHDOG_H
#define WATCHDOG_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <getopt.h>
#include <stdint.h>
#include <string.h>
#include <float.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <signal.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#define assert(expr) ((void)((expr) ? 0 : die("%s:%d: assertion %s failed in function %s",  __FILE__ , __LINE__, #expr, __func__)))

#define dp(level, ...) (dprintf(level, "%s:%d: %s ", __FILE__ , __LINE__, __func__),dprintf(level,  __VA_ARGS__))

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef enum action {
	A_SET,
	A_GET,
	A_ENABLE,
	A_DISABLE,
	A_RESET,
	A_SET_INPUT,
	A_SET_OUTPUT,
	A_DIRECTION,
	A_TEST,
} action_t;


#include "prototypes.h"
#endif
