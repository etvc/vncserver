/*
* $Id$
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2, or (at your option) any
* later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* This project is an adaptation of the original fbvncserver for the iPAQ
* and Zaurus.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/ioctl.h>

#include <sys/stat.h>
#include <sys/sysmacros.h>             /* For makedev() */

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>

#include <assert.h>
#include <errno.h>

/* libvncserver */
#include "rfb/rfb.h"
#include "rfb/keysym.h"
#include "vncconfig.h"
/*****************************************************************************/

static char FB_DEVICE[256] = "/dev/fb0";
static struct fb_var_screeninfo scrinfo;
static int fbfd = -1;
static unsigned short int *fbmmap = MAP_FAILED;
static unsigned short int *vncbuf;
static unsigned short int *fbbuf;

static int VNC_PORT = 5900;
static rfbScreenInfoPtr vncscr;


/* No idea, just copied from fbvncserver as part of the frame differerencing
* algorithm.  I will probably be later rewriting all of this. */
static struct varblock_t
{
	int min_i;
	int min_j;
	int max_i;
	int max_j;
	int r_offset;
	int g_offset;
	int b_offset;
	int rfb_xres;
	int rfb_maxy;
} varblock;

/*****************************************************************************/


static void init_fb(void)
{
	size_t pixels;
	size_t bytespp;

	if ((fbfd = open(FB_DEVICE, O_RDONLY)) == -1)
	{
		fprintf(stderr, "cannot open fb device %s\n", FB_DEVICE);
		exit(EXIT_FAILURE);
	}

	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &scrinfo) != 0)
	{
		fprintf(stderr, "ioctl error\n");
		exit(EXIT_FAILURE);
	}

	pixels = scrinfo.xres * scrinfo.yres;
	bytespp = scrinfo.bits_per_pixel / 8;

	fprintf(stderr, "xres=%d, yres=%d, xresv=%d, yresv=%d, xoffs=%d, yoffs=%d, bpp=%d\n",
		(int)scrinfo.xres, (int)scrinfo.yres,
		(int)scrinfo.xres_virtual, (int)scrinfo.yres_virtual,
		(int)scrinfo.xoffset, (int)scrinfo.yoffset,
		(int)scrinfo.bits_per_pixel);

	fbmmap = mmap(NULL, pixels * bytespp, PROT_READ, MAP_SHARED, fbfd, 0);

	if (fbmmap == MAP_FAILED)
	{
		fprintf(stderr, "mmap failed\n");
		exit(EXIT_FAILURE);
	}
}

static void cleanup_fb(void)
{
	if (fbfd != -1)
	{
		close(fbfd);
	}
}

/*****************************************************************************/

static void init_fb_server(int argc, char **argv)
{
	fprintf(stderr, "Initializing server...\n");
	/* Allocate the VNC server buffer to be managed (not manipulated) by
	* libvncserver. */
	vncbuf = calloc(scrinfo.xres * scrinfo.yres, scrinfo.bits_per_pixel / 8);
	assert(vncbuf != NULL);

	/* Allocate the comparison buffer for detecting drawing updates from frame
	* to frame. */
	fbbuf = calloc(scrinfo.xres * scrinfo.yres, scrinfo.bits_per_pixel / 8);
	assert(fbbuf != NULL);

	/* TODO: This assumes scrinfo.bits_per_pixel is 16. */
	vncscr = rfbGetScreen(&argc, argv, scrinfo.xres, scrinfo.yres, 5, 2, (scrinfo.bits_per_pixel / 8));
	assert(vncscr != NULL);

	vncscr->desktopName = GetTitleInfo();// "framebuffer";
	vncscr->frameBuffer = (char *)vncbuf;
	vncscr->alwaysShared = TRUE;
	vncscr->httpDir = NULL;
	vncscr->port = VNC_PORT;

	 //	vncscr->kbdAddEvent = keyevent;
	//	vncscr->ptrAddEvent = ptrevent;

	rfbInitServer(vncscr);

	/* Mark as dirty since we haven't sent any updates at all yet. */
	rfbMarkRectAsModified(vncscr, 0, 0, scrinfo.xres, scrinfo.yres);

	/* No idea. */
	varblock.r_offset = scrinfo.red.offset + scrinfo.red.length - 5;
	varblock.g_offset = scrinfo.green.offset + scrinfo.green.length - 5;
	varblock.b_offset = scrinfo.blue.offset + scrinfo.blue.length - 5;
	varblock.rfb_xres = scrinfo.yres;
	varblock.rfb_maxy = scrinfo.xres - 1;
}

/*****************************************************************************/

#define PIXEL_FB_TO_RFB(p,r,g,b) ((p>>r)&0x1f001f)|(((p>>g)&0x1f001f)<<5)|(((p>>b)&0x1f001f)<<10)

static void update_screen(void)
{
	unsigned int *f, *c, *r;
	int x, y;

	varblock.min_i = varblock.min_j = 9999;
	varblock.max_i = varblock.max_j = -1;

	f = (unsigned int *)fbmmap;        /* -> framebuffer         */
	c = (unsigned int *)fbbuf;         /* -> compare framebuffer */
	r = (unsigned int *)vncbuf;        /* -> remote framebuffer  */

	int multiplier = 1;
	if (scrinfo.bits_per_pixel == 32)
	{
		// HACK: support for 32 bit
		multiplier = 2;
	}
	for (y = 0; y < scrinfo.yres * multiplier; y++)
	{
		/* Compare every 2 pixels at a time, assuming that changes are likely
		* in pairs. */
		for (x = 0; x < scrinfo.xres; x += 2)
		{
			unsigned int pixel = *f;

			if (pixel != *c)
			{
				*c = pixel;

				/* XXX: Undo the checkered pattern to test the efficiency
				* gain using hextile encoding. */
				if (pixel == 0x18e320e4 || pixel == 0x20e418e3)
					pixel = 0x18e318e3;

				*r = PIXEL_FB_TO_RFB(pixel,
					varblock.r_offset, varblock.g_offset, varblock.b_offset);

				if (x < varblock.min_i)
					varblock.min_i = x;
				else
				{
					if (x > varblock.max_i)
						varblock.max_i = x;

					if (y > varblock.max_j)
						varblock.max_j = y;
					else if (y < varblock.min_j)
						varblock.min_j = y;
				}
			}

			f++, c++;
			r++;
		}
	}

	if (varblock.min_i < 9999)
	{
		if (varblock.max_i < 0)
			varblock.max_i = varblock.min_i;

		if (varblock.max_j < 0)
			varblock.max_j = varblock.min_j;

		fprintf(stderr, "Dirty page: %dx%d+%d+%d...\n",
			(varblock.max_i + 2) - varblock.min_i, (varblock.max_j + 1) - varblock.min_j,
			varblock.min_i, varblock.min_j);

		rfbMarkRectAsModified(vncscr, varblock.min_i, varblock.min_j,
			varblock.max_i + 2, varblock.max_j + 1);

		rfbProcessEvents(vncscr, 10000);
	}
}

/*****************************************************************************/

void print_usage(char **argv)
{
	fprintf(stderr, "%s [-f device] [-p port] [-h]\n"
		"-p port: VNC port, default is 5900\n"
		"-f device: framebuffer device node, default is /dev/fb0\n"
		"-h : print this help\n"
		, *argv);
}

int main(int argc, char **argv)
{
	if (argc > 1)
	{
		int i = 1;
		while (i < argc)
		{
			if (*argv[i] == '-')
			{
				switch (*(argv[i] + 1))
				{
				case 'h':
					print_usage(argv);
					exit(0);
					break;
				case 'f':
					i++;
					strcpy(FB_DEVICE, argv[i]);
					break;
				case 'p':
					i++;
					VNC_PORT = atoi(argv[i]);
					break;
				}
			}
			i++;
		}
	}
	VNCCONFIGINIT();
	fprintf(stderr, "Initializing framebuffer device %s...\n", FB_DEVICE);
	init_fb();

	fprintf(stderr, "Initializing VNC server:\n");
	fprintf(stderr, "	width:  %d\n", (int)scrinfo.xres);
	fprintf(stderr, "	height: %d\n", (int)scrinfo.yres);
	fprintf(stderr, "	bpp:    %d\n", (int)scrinfo.bits_per_pixel);
	fprintf(stderr, "	port:   %d\n", (int)VNC_PORT);
	init_fb_server(argc, argv);

	/* Implement our own event loop to detect changes in the framebuffer. */
	while (1)
	{
		while (vncscr->clientHead == NULL)
			rfbProcessEvents(vncscr, 100000);

		rfbProcessEvents(vncscr, 100000);
		update_screen();
	}

	fprintf(stderr, "Cleaning up...\n");
	cleanup_fb();
}

