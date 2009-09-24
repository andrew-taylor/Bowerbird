#ifndef SOUND_CAPTURE_H
#define SOUND_CAPTURE_H
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


#include "bowerbird.h"
#include "sound_capture-prototypes.h"

#endif
