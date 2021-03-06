/*
 *  V4L2 video capture example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>		/* getopt_long() */

#include <fcntl.h>		/* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>		/* for videodev2.h */

#include <linux/videodev2.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

typedef enum
{
  IO_METHOD_READ,
  IO_METHOD_MMAP,
  IO_METHOD_USERPTR,
} io_method;

struct buffer
{
  void *start;
  size_t length;
};

static char *dev_name = NULL;
static io_method io = IO_METHOD_MMAP;
	/* can be V4L2_PIX_FMT_YUV420 or V4L2_PIX_FMT_PWC2 */
static int pixel_format = V4L2_PIX_FMT_YUV420;
static int fd = -1;
struct buffer *buffers = NULL;
static unsigned int n_buffers = 0;
static unsigned int width = 640;
static unsigned int height = 480;
static unsigned int count = 10;

static void errno_exit(const char *s)
{
  fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
  exit(EXIT_FAILURE);
}

static int xioctl(int fd, int request, void *arg)
{
  int r;

  do
    r = ioctl(fd, request, arg);
  while (-1 == r && EINTR == errno);

  return r;
}
void write_jpeg(char *image, int width, int height, int quality, char *filename_format, ...);

static void process_image(const void *p, ssize_t size)
{
  static int no_image = 0;
  char filename[1024];
  int fd;
  ssize_t written = 0;

  snprintf(filename, sizeof(filename), "/tmp/webcam-%5.5d.%s",
	   no_image++, pixel_format == V4L2_PIX_FMT_YUV420 ? "yuv" : "raw");

  fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd < 0)
    {
      fputc('*', stdout);
      fflush(stdout);
      return;
    }
  do
    {
      int ret;
      ret = write(fd, p + written, size - written);
      if (ret < 0)
	{
	  fputc('+', stdout);
	  fflush(stdout);
	  return;
	}
      written += ret;
    }
  while (written < size);
  close(fd);

  fputc('.', stdout);
  fflush(stdout);
	write_jpeg((void *)p, width, height, 90, "/tmp/webcam-%5.5d.jpg", no_image-1);
}

static int read_frame(void)
{
  struct v4l2_buffer buf;
  unsigned int i;
  ssize_t read_bytes;
  unsigned int total_read_bytes;

  switch (io)
    {
    case IO_METHOD_READ:
      total_read_bytes = 0;
      do
       {
	 read_bytes = read(fd, buffers[0].start, buffers[0].length);
	 if (read_bytes < 0)
	  {
	    switch (errno)
	     {
	      case EIO:
	      case EAGAIN:
		continue;
	      default:
		errno_exit("read");
	     }
	  }
	 total_read_bytes += read_bytes;

       } while (total_read_bytes < buffers[0].length);
      process_image(buffers[0].start, buffers[0].length);

      break;

    case IO_METHOD_MMAP:
      CLEAR(buf);

      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;

      if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
	{
	  switch (errno)
	    {
	    case EAGAIN:
	      return 0;

	    case EIO:
	      /* Could ignore EIO, see spec. */

	      /* fall through */

	    default:
	      errno_exit("VIDIOC_DQBUF");
	    }
	}

      assert(buf.index < n_buffers);

      process_image(buffers[buf.index].start, buf.length);

      if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
	errno_exit("VIDIOC_QBUF");

      break;

    case IO_METHOD_USERPTR:
      CLEAR(buf);

      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_USERPTR;

      if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
	{
	  switch (errno)
	    {
	    case EAGAIN:
	      return 0;

	    case EIO:
	      /* Could ignore EIO, see spec. */

	      /* fall through */

	    default:
	      errno_exit("VIDIOC_DQBUF");
	    }
	}

      for (i = 0; i < n_buffers; ++i)
	if (buf.m.userptr == (unsigned long) buffers[i].start
	    && buf.length == buffers[i].length)
	  break;

      assert(i < n_buffers);

      process_image((void *) buf.m.userptr, buf.length);

      if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
	errno_exit("VIDIOC_QBUF");

      break;
    }

  return 1;
}

static void mainloop(void)
{
  while (count-- > 0)
    {
      for (;;)
	{
	  fd_set fds;
	  struct timeval tv;
	  int r;

	  FD_ZERO(&fds);
	  FD_SET(fd, &fds);

	  /* Timeout. */
	  tv.tv_sec = 2;
	  tv.tv_usec = 0;

	  r = select(fd + 1, &fds, NULL, NULL, &tv);

	  if (-1 == r)
	    {
	      if (EINTR == errno)
		continue;

	      errno_exit("select");
	    }

	  if (0 == r)
	    {
	      fprintf(stderr, "select timeout\n");
	      exit(EXIT_FAILURE);
	    }

	  if (read_frame())
	    break;

	  /* EAGAIN - continue select loop. */
	}
    }
}

static void stop_capturing(void)
{
  enum v4l2_buf_type type;

  switch (io)
    {
    case IO_METHOD_READ:
      /* Nothing to do. */
      break;

    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
	errno_exit("VIDIOC_STREAMOFF");

      break;
    }
}

static void start_capturing(void)
{
  unsigned int i;
  enum v4l2_buf_type type;

  switch (io)
    {
    case IO_METHOD_READ:
      /* Nothing to do. */
      break;

    case IO_METHOD_MMAP:
      for (i = 0; i < n_buffers; ++i)
	{
	  struct v4l2_buffer buf;

	  CLEAR(buf);

	  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	  buf.memory = V4L2_MEMORY_MMAP;
	  buf.index = i;

	  if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
	    errno_exit("VIDIOC_QBUF");
	}

      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
	errno_exit("VIDIOC_STREAMON");

      break;

    case IO_METHOD_USERPTR:
      for (i = 0; i < n_buffers; ++i)
	{
	  struct v4l2_buffer buf;

	  CLEAR(buf);

	  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	  buf.memory = V4L2_MEMORY_USERPTR;
	  buf.m.userptr = (unsigned long) buffers[i].start;
	  buf.length = buffers[i].length;

	  if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
	    errno_exit("VIDIOC_QBUF");
	}


      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
	errno_exit("VIDIOC_STREAMON");

      break;
    }
}

static void uninit_device(void)
{
  unsigned int i;

  switch (io)
    {
    case IO_METHOD_READ:
      free(buffers[0].start);
      break;

    case IO_METHOD_MMAP:
      for (i = 0; i < n_buffers; ++i)
	if (-1 == munmap(buffers[i].start, buffers[i].length))
	  errno_exit("munmap");
      break;

    case IO_METHOD_USERPTR:
      for (i = 0; i < n_buffers; ++i)
	free(buffers[i].start);
      break;
    }

  free(buffers);
}

static void init_read(unsigned int buffer_size)
{
  buffers = calloc(1, sizeof(*buffers));

  if (!buffers)
    {
      fprintf(stderr, "Out of memory\n");
      exit(EXIT_FAILURE);
    }

  buffers[0].length = buffer_size;
  buffers[0].start = malloc(buffer_size);

  if (!buffers[0].start)
    {
      fprintf(stderr, "Out of memory\n");
      exit(EXIT_FAILURE);
    }
}

static void init_mmap(void)
{
  struct v4l2_requestbuffers req;

  CLEAR(req);

  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
    {
      if (EINVAL == errno)
	{
	  fprintf(stderr, "%s does not support "
		  "memory mapping\n", dev_name);
	  exit(EXIT_FAILURE);
	}
      else
	{
	  errno_exit("VIDIOC_REQBUFS");
	}
    }

  if (req.count < 2)
    {
      fprintf(stderr, "Insufficient buffer memory on %s\n", dev_name);
      exit(EXIT_FAILURE);
    }

  buffers = calloc(req.count, sizeof(*buffers));

  if (!buffers)
    {
      fprintf(stderr, "Out of memory\n");
      exit(EXIT_FAILURE);
    }

  for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
    {
      struct v4l2_buffer buf;

      CLEAR(buf);

      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.index = n_buffers;

      if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
	errno_exit("VIDIOC_QUERYBUF");

      buffers[n_buffers].length = buf.length;
      buffers[n_buffers].start = mmap(NULL /* start anywhere */ ,
				      buf.length,
				      PROT_READ | PROT_WRITE /* required */ ,
				      MAP_SHARED /* recommended */ ,
				      fd, buf.m.offset);

      if (MAP_FAILED == buffers[n_buffers].start)
	errno_exit("mmap");
    }
}

static void init_userp(unsigned int buffer_size)
{
  struct v4l2_requestbuffers req;

  CLEAR(req);

  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_USERPTR;

  if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
    {
      if (EINVAL == errno)
	{
	  fprintf(stderr, "%s does not support "
		  "user pointer i/o\n", dev_name);
	  exit(EXIT_FAILURE);
	}
      else
	{
	  errno_exit("VIDIOC_REQBUFS");
	}
    }

  buffers = calloc(4, sizeof(*buffers));

  if (!buffers)
    {
      fprintf(stderr, "Out of memory\n");
      exit(EXIT_FAILURE);
    }

  for (n_buffers = 0; n_buffers < 4; ++n_buffers)
    {
      buffers[n_buffers].length = buffer_size;
      buffers[n_buffers].start = malloc(buffer_size);

      if (!buffers[n_buffers].start)
	{
	  fprintf(stderr, "Out of memory\n");
	  exit(EXIT_FAILURE);
	}
    }
}

static void init_device(void)
{
  struct v4l2_capability cap;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  struct v4l2_format fmt;
  unsigned int min;

  if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap))
    {
      if (EINVAL == errno)
	{
	  fprintf(stderr, "%s is no V4L2 device\n", dev_name);
	  exit(EXIT_FAILURE);
	}
      else
	{
	  errno_exit("VIDIOC_QUERYCAP");
	}
    }

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
      fprintf(stderr, "%s is no video capture device\n", dev_name);
      exit(EXIT_FAILURE);
    }

  switch (io)
    {
    case IO_METHOD_READ:
      if (!(cap.capabilities & V4L2_CAP_READWRITE))
	{
	  fprintf(stderr, "%s does not support read i/o\n", dev_name);
	  exit(EXIT_FAILURE);
	}

      break;

    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
      if (!(cap.capabilities & V4L2_CAP_STREAMING))
	{
	  fprintf(stderr, "%s does not support streaming i/o\n", dev_name);
	  exit(EXIT_FAILURE);
	}

      break;
    }

  /* Select video input, video standard and tune here. */

  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (-1 == xioctl(fd, VIDIOC_CROPCAP, &cropcap))
    {
      /* Errors ignored. */
    }

  crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  crop.c = cropcap.defrect;	/* reset to default */

  if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop))
    {
      switch (errno)
	{
	case EINVAL:
	  /* Cropping not supported. */
	  break;
	default:
	  /* Errors ignored. */
	  break;
	}
    }

  CLEAR(fmt);

  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = width;
  fmt.fmt.pix.height = height;
  fmt.fmt.pix.pixelformat = pixel_format;
  fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

  if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
    errno_exit("VIDIOC_S_FMT");

  /* Note VIDIOC_S_FMT may change width and height. */
  width = fmt.fmt.pix.width;
  height = fmt.fmt.pix.height;

  switch (io)
    {
    case IO_METHOD_READ:
      init_read(fmt.fmt.pix.sizeimage);
      break;

    case IO_METHOD_MMAP:
      init_mmap();
      break;

    case IO_METHOD_USERPTR:
      init_userp(fmt.fmt.pix.sizeimage);
      break;
    }
}

static void close_device(void)
{
  if (-1 == close(fd))
    errno_exit("close");

  fd = -1;
}

static void open_device(void)
{
  struct stat st;

  if (-1 == stat(dev_name, &st))
    {
      fprintf(stderr, "Cannot identify '%s': %d, %s\n",
	      dev_name, errno, strerror(errno));
      exit(EXIT_FAILURE);
    }

  if (!S_ISCHR(st.st_mode))
    {
      fprintf(stderr, "%s is no device\n", dev_name);
      exit(EXIT_FAILURE);
    }

  fd = open(dev_name, O_RDWR /* required */  | O_NONBLOCK, 0);

  if (-1 == fd)
    {
      fprintf(stderr, "Cannot open '%s': %d, %s\n",
	      dev_name, errno, strerror(errno));
      exit(EXIT_FAILURE);
    }
}

static void usage(FILE * fp, int argc, char **argv)
{
  fprintf(fp,
	  "Usage: %s [options]\n\n"
	  "Options:\n"
	  "-c | --count COUNT   Number of frames do grab\n"
	  "-d | --device name   Video device name [/dev/video0]\n"
	  "-f | --format FORMAT yuv (YUV420P) or raw(native)\n"
	  "-h | --help          Print this message\n"
	  "-m | --mmap          Use memory mapped buffers\n"
	  "-r | --read          Use read() calls\n"
	  "-s | --size   WxH    WIDTHxHEIGHT\n"
	  "-u | --userp         Use application allocated buffers\n"
	  "", argv[0]);
}

static const char short_options[] = "c:d:f:hmrs:u";

static const struct option long_options[] = {
  {"count", required_argument, NULL, 'c'},
  {"device", required_argument, NULL, 'd'},
  {"format", required_argument, NULL, 'f'},
  {"help", no_argument, NULL, 'h'},
  {"mmap", no_argument, NULL, 'm'},
  {"read", no_argument, NULL, 'r'},
  {"size", required_argument, NULL, 's'},
  {"userp", no_argument, NULL, 'u'},
  {0, 0, 0, 0}
};

int main(int argc, char **argv)
{
  dev_name = "/dev/video0";

  for (;;)
    {
      int index;
      int c;

      c = getopt_long(argc, argv, short_options, long_options, &index);

      if (-1 == c)
	break;

      switch (c)
	{
	case 0:		/* getopt_long() flag */
	  break;

	case 'c':
	  if (sscanf(optarg, "%u", &count)!=1) {
	     printf("Bad count specified {%s}\n", optarg);
	     usage(stdout, argc, argv);
	     exit(EXIT_FAILURE);
	  }
	  break;
	case 'd':
	  dev_name = optarg;
	  break;

	case 'f':
	  if (strcmp(optarg, "yuv") == 0)
	    pixel_format = V4L2_PIX_FMT_YUV420;
	  else if (strcmp(optarg, "raw") == 0)
	    pixel_format = V4L2_PIX_FMT_PWC2;
	  else
	    {
	      printf ("Bad format specified {%s} "
		      "can be either 'yuv' or 'raw'\n", optarg);
	      usage(stdout, argc, argv);
	      exit(EXIT_FAILURE);
	    }
	  break;

	case 'h':
	  usage(stdout, argc, argv);
	  exit(EXIT_SUCCESS);

	case 'm':
	  io = IO_METHOD_MMAP;
	  break;

	case 'r':
	  io = IO_METHOD_READ;
	  break;

	case 's':
	  if (sscanf(optarg, "%dx%d", &width, &height) != 2)
	    {
	      printf("Bad size specified {%s}\n", optarg);
	      usage(stdout, argc, argv);
	      exit(EXIT_FAILURE);
	    }
	  break;
	case 'u':
	  io = IO_METHOD_USERPTR;
	  break;

	default:
	  usage(stderr, argc, argv);
	  exit(EXIT_FAILURE);
	}
    }

  open_device();

  init_device();

  start_capturing();

  mainloop();

  stop_capturing();

  uninit_device();

  close_device();

  exit(EXIT_SUCCESS);

  return 0;
}

#include "jpeglib.h"
#include <stdarg.h>

/*
 *
 *	Copyright 2002 by Jeroen Vreeken (pe1rxq@amsat.org)
 *	Portions of this file are Copyright by Lionnel Maugis
 *	This software is distributed under the GNU public license version 2
 *	See also the file 'COPYING'.
 *
 */
// twice as fast as RGB jpeg 
void
put_jpeg_yuv420p(FILE *fp, unsigned char *image, int width, int height, int quality)
{
	int i,j;

	JSAMPROW y[16],cb[16],cr[16]; // y[2][5] = color sample of row 2 and pixel column 5; (one plane)
	JSAMPARRAY data[3]; // t[0][2][5] = color sample 0 of row 2 and column 5

	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;

	data[0] = y;
	data[1] = cb;
	data[2] = cr;

	cinfo.err = jpeg_std_error(&jerr);  // errors get written to stderr 
	
	jpeg_create_compress (&cinfo);
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;
	jpeg_set_defaults (&cinfo);

	jpeg_set_colorspace(&cinfo, JCS_YCbCr);

	cinfo.raw_data_in = TRUE; // supply downsampled data
	cinfo.comp_info[0].h_samp_factor = 2;
	cinfo.comp_info[0].v_samp_factor = 2;
	cinfo.comp_info[1].h_samp_factor = 1;
	cinfo.comp_info[1].v_samp_factor = 1;
	cinfo.comp_info[2].h_samp_factor = 1;
	cinfo.comp_info[2].v_samp_factor = 1;

	jpeg_set_quality (&cinfo, quality, TRUE);
	cinfo.dct_method = JDCT_FASTEST;

	jpeg_stdio_dest (&cinfo, fp);  	  // data written to file
	jpeg_start_compress (&cinfo, TRUE);

	for (j=0;j<height;j+=16) {
		for (i=0;i<16;i++) {
			y[i] = image + width*(i+j);
			if (i%2 == 0) {
				cb[i/2] = image + width*height + width/2*((i+j)/2);
				cr[i/2] = image + width*height + width*height/4 + width/2*((i+j)/2);
			}
		}
		jpeg_write_raw_data (&cinfo, data, 16);
	}

	jpeg_finish_compress (&cinfo);
	jpeg_destroy_compress (&cinfo);
}

void
write_jpeg(char *image, int width, int height, int quality, char *filename_format, ...) {
	char filename[4096];
	va_list ap;
	va_start(ap, filename_format);
	vsnprintf(filename, sizeof filename, filename_format, ap);
	FILE *f = fopen(filename, "w");
	assert(f != NULL);
//	dp(1, "writing %s\n", filename);
	put_jpeg_yuv420p(f, image, width, height, quality);
	fclose(f);
}
