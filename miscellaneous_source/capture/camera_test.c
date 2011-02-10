#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/wait.h>
#include <linux/videodev.h>

#include "pwc-ioctl.h"
void pwc_ioctl(char *option_name, int argument, int which_ioctl);

int capture_device_fd;
int
main(int argc,  char *argv[]) {
	char *capture_device = "/dev/video0";
	int height = 480, width = 640;
	int pwc_frame_rate = 5;
	
	assert((capture_device_fd = open(capture_device, O_RDWR)) >= 0);
	struct video_window vid_win;
	assert(ioctl(capture_device_fd, VIDIOCGWIN, &vid_win) != -1);
	vid_win.width = width;
	vid_win.height = height;
	assert(ioctl(capture_device_fd, VIDIOCSWIN, &vid_win) != -1);
	assert(vid_win.flags & PWC_FPS_FRMASK);
	fprintf(stderr, "Setting frame rate to %d fps (was %d fps)\n",  pwc_frame_rate, (vid_win.flags & PWC_FPS_FRMASK) >> PWC_FPS_SHIFT);
	vid_win.flags &= ~PWC_FPS_FRMASK;
	vid_win.flags |= (pwc_frame_rate << PWC_FPS_SHIFT);
	assert(ioctl(capture_device_fd, VIDIOCGWIN, &vid_win) != -1);	
	assert(ioctl(capture_device_fd, VIDIOCSWIN, &vid_win) != -1);	
	assert(ioctl(capture_device_fd, VIDIOCPWCSAWB, &(struct pwc_whitebalance){ PWC_WB_AUTO}) != -1);	
	assert(ioctl(capture_device_fd, VIDIOCPWCSLED, &(struct pwc_leds){ 0}) != -1);	
	pwc_ioctl("pwc_compression",  3, VIDIOCPWCSCQUAL);
	pwc_ioctl("pwc_agc",  -1, VIDIOCPWCSAGC);
	pwc_ioctl("pwc_backlight",  0, VIDIOCPWCSBACKLIGHT);
	pwc_ioctl("pwc_antiflicker", 0,  VIDIOCPWCSFLICKER);
	pwc_ioctl("pwc_noise_reduction",  3,  VIDIOCPWCSDYNNOISE);
}

void
pwc_ioctl(char *option_name, int argument, int which_ioctl) {
	fprintf(stderr, "Setting %s to %d\n",  option_name, argument);
	assert(ioctl(capture_device_fd, which_ioctl, &argument) != -1);	
}
