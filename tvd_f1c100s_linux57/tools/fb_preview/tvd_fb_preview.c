#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/videodev2.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct video_buffer {
	void *start;
	size_t length;
};

static volatile sig_atomic_t stop;

static void on_signal(int signal)
{
	(void)signal;
	stop = 1;
}

static int xioctl(int fd, unsigned long request, void *arg)
{
	int ret;

	do {
		ret = ioctl(fd, request, arg);
	} while (ret < 0 && errno == EINTR);

	return ret;
}

static uint8_t clip_u8(int value)
{
	if (value < 0)
		return 0;
	if (value > 255)
		return 255;
	return value;
}

static uint16_t rgb_to_rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
	return ((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3);
}

static uint16_t nv16_pixel_to_rgb565(const uint8_t *y_plane,
				     const uint8_t *uv_plane,
				     unsigned int src_width,
				     unsigned int x,
				     unsigned int y)
{
	int yy;
	int cb;
	int cr;
	int c;
	int d;
	int e;
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	unsigned int uv_x = x & ~1U;

	yy = y_plane[y * src_width + x];
	cb = uv_plane[y * src_width + uv_x + 0];
	cr = uv_plane[y * src_width + uv_x + 1];

	c = yy - 16;
	d = cb - 128;
	e = cr - 128;
	if (c < 0)
		c = 0;

	red = clip_u8((298 * c + 409 * e + 128) >> 8);
	green = clip_u8((298 * c - 100 * d - 208 * e + 128) >> 8);
	blue = clip_u8((298 * c + 516 * d + 128) >> 8);

	return rgb_to_rgb565(red, green, blue);
}

static void render_nv16_to_rgb565(uint16_t *fb,
				  unsigned int fb_width,
				  unsigned int fb_height,
				  unsigned int fb_stride_pixels,
				  const uint8_t *frame,
				  unsigned int src_width,
				  unsigned int src_height)
{
	const uint8_t *y_plane = frame;
	const uint8_t *uv_plane = frame + src_width * src_height;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int view_width = fb_width;
	unsigned int view_height = fb_height;

	for (dst_y = 0; dst_y < view_height; dst_y++) {
		uint16_t *out = fb + dst_y * fb_stride_pixels;
		unsigned int src_y = dst_y * src_height / view_height;

		for (dst_x = 0; dst_x < view_width; dst_x++) {
			unsigned int src_x = dst_x * src_width / view_width;

			out[dst_x] = nv16_pixel_to_rgb565(y_plane, uv_plane,
							  src_width, src_x, src_y);
		}
	}
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [-d /dev/videoX] [-f /dev/fb0] [-s pal|ntsc]\n"
		"Defaults: /dev/video7, /dev/fb0, pal\n",
		prog);
}

int main(int argc, char **argv)
{
	const char *video_path = "/dev/video7";
	const char *fb_path = "/dev/fb0";
	const char *standard = "pal";
	unsigned int src_width = 720;
	unsigned int src_height = 576;
	int video_fd = -1;
	int fb_fd = -1;
	struct video_buffer buffers[4];
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	void *fb_mem = MAP_FAILED;
	size_t fb_size;
	unsigned int fb_stride_pixels;
	enum v4l2_buf_type type;
	int opt;
	int ret = 1;
	unsigned int index;

	memset(buffers, 0, sizeof(buffers));

	while ((opt = getopt(argc, argv, "d:f:s:h")) != -1) {
		switch (opt) {
		case 'd':
			video_path = optarg;
			break;
		case 'f':
			fb_path = optarg;
			break;
		case 's':
			standard = optarg;
			break;
		case 'h':
		default:
			usage(argv[0]);
			return opt == 'h' ? 0 : 1;
		}
	}

	if (!strcmp(standard, "ntsc")) {
		src_height = 480;
	} else if (!strcmp(standard, "pal")) {
		src_height = 576;
	} else {
		usage(argv[0]);
		return 1;
	}

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	video_fd = open(video_path, O_RDWR | O_NONBLOCK);
	if (video_fd < 0) {
		perror(video_path);
		goto out;
	}

	fb_fd = open(fb_path, O_RDWR);
	if (fb_fd < 0) {
		perror(fb_path);
		goto out;
	}

	if (xioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		perror("FBIOGET_FSCREENINFO");
		goto out;
	}

	if (xioctl(fb_fd, FBIOGET_VSCREENINFO, &var) < 0) {
		perror("FBIOGET_VSCREENINFO");
		goto out;
	}

	if (var.bits_per_pixel != 16) {
		fprintf(stderr, "Only RGB565 fb is supported, got %u bpp\n",
			var.bits_per_pixel);
		goto out;
	}

	fb_size = fix.smem_len;
	fb_stride_pixels = fix.line_length / 2;
	fb_mem = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if (fb_mem == MAP_FAILED) {
		perror("mmap fb");
		goto out;
	}

	{
		v4l2_std_id std_id = !strcmp(standard, "pal") ? V4L2_STD_PAL : V4L2_STD_NTSC;
		struct v4l2_format fmt;

		if (xioctl(video_fd, VIDIOC_S_STD, &std_id) < 0)
			perror("VIDIOC_S_STD");

		memset(&fmt, 0, sizeof(fmt));
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.width = src_width;
		fmt.fmt.pix.height = src_height;
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV16;
		fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
		if (xioctl(video_fd, VIDIOC_S_FMT, &fmt) < 0) {
			perror("VIDIOC_S_FMT");
			goto out;
		}

		src_width = fmt.fmt.pix.width;
		src_height = fmt.fmt.pix.height;
	}

	{
		struct v4l2_requestbuffers req;
		unsigned int index;

		memset(&req, 0, sizeof(req));
		req.count = ARRAY_SIZE(buffers);
		req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		req.memory = V4L2_MEMORY_MMAP;
		if (xioctl(video_fd, VIDIOC_REQBUFS, &req) < 0) {
			perror("VIDIOC_REQBUFS");
			goto out;
		}

		if (req.count < 2) {
			fprintf(stderr, "Not enough V4L2 buffers\n");
			goto out;
		}

		for (index = 0; index < req.count; index++) {
			struct v4l2_buffer buf;

			memset(&buf, 0, sizeof(buf));
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = index;
			if (xioctl(video_fd, VIDIOC_QUERYBUF, &buf) < 0) {
				perror("VIDIOC_QUERYBUF");
				goto out;
			}

			buffers[index].length = buf.length;
			buffers[index].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
						    MAP_SHARED, video_fd, buf.m.offset);
			if (buffers[index].start == MAP_FAILED) {
				perror("mmap video");
				goto out;
			}

			if (xioctl(video_fd, VIDIOC_QBUF, &buf) < 0) {
				perror("VIDIOC_QBUF");
				goto out;
			}
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(video_fd, VIDIOC_STREAMON, &type) < 0) {
		perror("VIDIOC_STREAMON");
		goto out;
	}

	fprintf(stderr, "Preview %s %ux%u -> %ux%u RGB565. Ctrl+C to stop.\n",
		standard, src_width, src_height, var.xres, var.yres);

	while (!stop) {
		struct timeval tv;
		fd_set fds;
		struct v4l2_buffer buf;

		FD_ZERO(&fds);
		FD_SET(video_fd, &fds);
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		if (select(video_fd + 1, &fds, NULL, NULL, &tv) <= 0)
			continue;

		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		if (xioctl(video_fd, VIDIOC_DQBUF, &buf) < 0) {
			if (errno == EAGAIN)
				continue;
			perror("VIDIOC_DQBUF");
			break;
		}

		if (buf.index < ARRAY_SIZE(buffers)) {
			render_nv16_to_rgb565((uint16_t *)fb_mem, var.xres, var.yres,
					      fb_stride_pixels, buffers[buf.index].start,
					      src_width, src_height);
		}

		if (xioctl(video_fd, VIDIOC_QBUF, &buf) < 0) {
			perror("VIDIOC_QBUF");
			break;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	xioctl(video_fd, VIDIOC_STREAMOFF, &type);
	ret = 0;

out:
	if (fb_mem != MAP_FAILED)
		munmap(fb_mem, fb_size);

	for (index = 0; index < ARRAY_SIZE(buffers); index++) {
		if (buffers[index].start && buffers[index].start != MAP_FAILED)
			munmap(buffers[index].start, buffers[index].length);
	}

	if (fb_fd >= 0)
		close(fb_fd);
	if (video_fd >= 0)
		close(video_fd);

	return ret;
}
