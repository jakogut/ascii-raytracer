#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/*
 * ANSI/VT100 escape codes
 * http://www.termsys.demon.co.uk/vtansi.htm
 */
#define TTY_ERASE_SCREEN 	"\e[2J"
#define TTY_CURSOR_HOME		"\e[H"

#define STDOUT_PATH "/proc/self/fd/0"

// http://paulbourke.net/dataformats/asciiart/
#define GRAYSCALE_CHAR_MAP "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. "
#define FPS 2

/* You can create this image with Gimp, exporting as a C header
 */
#include "image.h"

char *get_current_pty_path()
{
	const size_t pty_path_bufsize = 4095;
	char *pty_path_buf = calloc(1, pty_path_bufsize);
	ssize_t ret = readlink(STDOUT_PATH, pty_path_buf, pty_path_bufsize);
	if (ret < 0) {
		free(pty_path_buf);
		return NULL;
	} else {
		return pty_path_buf;
	}
	
}

int tty_clear(int tty)
{
	char *seq = TTY_CURSOR_HOME
		    TTY_ERASE_SCREEN;

	return write(tty, seq, strlen(seq));
}

int get_tty_fd()
{
	int tty = -1;
	char *pty_path;
	if (pty_path = get_current_pty_path()) {
		tty = open(pty_path, O_RDWR);
		free(pty_path);
	}

	return tty;
}

float rgb_to_luma(uint8_t *rgb)
{
	/* Calcuate RGB -> Luma
	 * https://stackoverflow.com/a/596241/136846
	 */
	return 0.299 * rgb[0]
	     + 0.587 * rgb[1]
	     + 0.114 * rgb[2];	
}

#ifndef __EMSCRIPTEN__
void update(int tty, char *fb, struct timespec *interval)
{
	tty_clear(tty);
	write(tty, fb, strlen(fb));
	nanosleep(interval, NULL);
}
#else
struct update_args {
	int tty;
	char *fb;
};

void update(void *pargs)
{
	struct update_args *args = pargs;
	tty_clear(args->tty);
	write(args->tty, args->fb, strlen(args->fb));
}
#endif

int main()
{
	int tty;

	extern unsigned int width;
	extern unsigned int height;

	size_t frame_size = (width + 1) * (height / 2);
	char framebuffer[frame_size + 1];

	uint8_t rgb[3];
	float row_luma[width],
	      scaled_luma;
	uint8_t char_idx;
	for (int y = 0; y < height / 2; y++) {
		memset(row_luma, 0, sizeof(float) * width);

		/* Sample two rows at a time, to make up for the rectangular
		 * aspect ratio of most fonts.
		 */
		for (int x = 0; x < width * 2; x++) {
			HEADER_PIXEL(header_data, rgb);
			row_luma[x % width] += rgb_to_luma(rgb);
		}

		for (int x = 0; x < width; x++) {
			scaled_luma = (row_luma[x] / 2)
				    / 255.0f
				    * (strlen(GRAYSCALE_CHAR_MAP) - 1);
			
			char_idx = scaled_luma + 0.5;
			framebuffer[y * width + x] = GRAYSCALE_CHAR_MAP[char_idx];
		}

		framebuffer[((y + 1) * width) - 1] = '\n';
	}

	struct timespec interval = {
		.tv_sec = 0,
		.tv_nsec = 1000000000 / FPS,
	};

	if ((tty = get_tty_fd()) < 0)
		return tty;

#ifndef __EMSCRIPTEN__
	while (1) {
		update(tty, framebuffer, &interval);
	}
#else
	struct update_args args = {
		.tty = tty,
		.fb = framebuffer,
	};

	emscripten_set_main_loop_arg(update, &args, FPS, 0);
#endif

	return 0;
}
