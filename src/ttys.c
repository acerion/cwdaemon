/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2015 Kamil Ignacak <acerion@wp.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#define _DEFAULT_SOURCE /* S_IFMT and related. TODO (acerion) 2021.12.12: check validity of this comment. */

#include "config.h"

# if HAVE_STDIO_H
# include <stdio.h>
#endif
#if STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_TERMIOS_H
# include <termios.h>
#endif
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#if (defined(__unix__) || defined(unix)) && !defined(USG)
# include <sys/param.h>
#endif

#include "cwdaemon.h"

/**
   \file ttys.c

   Serial port functions.
*/


/**
   \brief Get fd for serial port

   Check to see whether \p fname is a tty type character device
   capable of TIOCM*.
   This should be platform independent.

   \return -1 if the device isn't a suitable tty device
   \return a file descriptor if the device is a suitable tty device
*/
int dev_get_tty(const char *fname)
{
	char nm[MAXPATHLEN];
	struct stat st;
	int fd, m;

	m = snprintf(nm, sizeof(nm), "/dev/%s", fname);
	if (m <= 0 || (unsigned int) m >= sizeof(nm)) {
		return -1;
	}

	if ((fd = open(nm, O_RDWR | O_NOCTTY | O_NONBLOCK)) == -1)
		return (-1);
	if (fstat(fd, &st) == -1)
		goto out;
	if ((st.st_mode & S_IFMT) != S_IFCHR)
		goto out;
	if (ioctl(fd, TIOCMGET, &m) == -1)
		goto out;
	return (fd);
out:
	close(fd);
	return (-1);

}

int
ttys_init (cwdevice * dev, int fd)
{

	dev->fd = fd;
	dev->reset (dev);
	return 0;
}

int
ttys_free (cwdevice * dev)
{
	dev->reset (dev);
	close (dev->fd);
	return 0;
}

int
ttys_reset (cwdevice * dev)
{
	ttys_cw (dev, OFF);
	ttys_ptt (dev, OFF);
	return 0;
}


/* CW keying - bit1 (pin 4 for DB-9) */
int
ttys_cw (cwdevice * dev, int onoff)
{
	int result, y = TIOCM_DTR;

	result = ioctl (dev->fd, onoff ? TIOCMBIS : TIOCMBIC, &y);
	if (result < 0)
	{
		cwdaemon_errmsg("Ioctl serial port %s", dev->desc);
		exit (1);
	}
	return 0;
}

/* SSB PTT keying - 0 bit (pin 7 for DB-9) */
int
ttys_ptt (cwdevice * dev, int onoff)
{
	int result, y = TIOCM_RTS;

	result = ioctl (dev->fd, onoff ? TIOCMBIS : TIOCMBIC, &y);
	if (result < 0)
	{
		cwdaemon_errmsg("Ioctl serial port %s", dev->desc);
		exit (1);
	}
	return 0;
}
