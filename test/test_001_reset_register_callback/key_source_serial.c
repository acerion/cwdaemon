#define _GNU_SOURCE /* strerror_r() */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include "key_source_serial.h"




static const char * g_device = "/dev/ttyS0";




bool key_source_open(cw_key_source_t * source)
{
	/* Open serial port. */
	errno = 0;
	int fd = open(g_device, O_RDONLY);
	if (fd == -1) {
		char buf[32] = { 0 };
		strerror_r(errno, buf, sizeof (buf));
		fprintf(stderr, "[EE]: open(%s): %s\n", g_device, buf);
		return false;
	}
	source->inner = (uintptr_t) fd;

	return true;
}




void key_source_close(cw_key_source_t * source)
{
	int fd = (int) source->inner;
	close(fd);
}



