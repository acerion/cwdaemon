/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *                      and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2022 Kamil Ignacak <acerion@wp.pl>
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

/* Comments from Wolf-Ruediger Juergens, DL2WRJ
 * very helpful: http://people.redhat.com/twaugh/parport/html/x916.html
 * and: http://www.xml.com/ldd/chapter/book/ch08.html
 * (Rubini et al. Linux Device Driver Book)
 */

#define _DEFAULT_SOURCE /* S_IFMT and related. TODO (acerion) 2021.12.12: check validity of this comment. */

#include "config.h"

# if HAVE_STDIO_H
# include <stdio.h>
#endif
#if STDC_HEADERS
# include <stddef.h>
# include <stdlib.h>
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
#ifdef HAVE_LINUX_PPDEV_H
# include <linux/parport.h>
# include <linux/ppdev.h>
#endif
#ifdef HAVE_DEV_PPBUS_PPI_H
# include <dev/ppbus/ppi.h>
# include <dev/ppbus/ppbconf.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#if (defined(__unix__) || defined(unix)) && !defined(USG)
# include <sys/param.h>
#endif

#include <errno.h>
#include <string.h>

#include "cwdaemon.h"
#include "log.h"
#include "lp.h"
#include "utils.h"




/**
   \file lp.c - parport functions
*/


#if defined(HAVE_LINUX_PPDEV_H)                /* Linux (ppdev) */
/**
   \brief Check to see whether \p fname is a parallel port type character device.

   Unfortunately, this is platform specific.

   \return -1 if the device is not suitable for use as parallel port based keyer
   \return a file descriptor if the device is suitable for use as a parallel port based keyer
*/
int dev_get_parport(const char *fname)
{
	char nm[MAXPATHLEN];
	struct stat st;
	int m = 0;

	int retv = build_full_device_path(nm, sizeof (nm), fname);
	if (0 != retv) {
		log_message(LOG_ERR, "Can't build path of lp device from [%s]: %s", fname, strerror(-retv));
		return -1;
	}
	int fd = open(nm, O_RDWR | O_NONBLOCK);
	if (fd == -1) {
		log_message(LOG_ERR, "open() failed for lp device [%s]: %s", nm, strerror(errno)); // TODO 2023.04.25: change this to NOTICE
		return (-1);
	}
	if (fstat(fd, &st) == -1) {
		log_message(LOG_ERR, "fstat() failed for lp device [%s]: %s", nm, strerror(errno));
		goto out;
	}
	if ((st.st_mode & S_IFMT) != S_IFCHR) {
		log_message(LOG_ERR, "lp device [%s] is not character device", nm);
		goto out;
	}
	if (ioctl(fd, PPGETMODE, &m) == -1) {
		log_message(LOG_ERR, "ioctl(PPGETMODE) failed for lp device [%s]: %s", nm, strerror(errno));
		goto out;
	}
	return (fd);
out:
	close(fd);
	return (-1);
}

#elif defined(HAVE_DEV_PPBUS_PPI_H)    /* FreeBSD (ppbus/ppi) */

int dev_get_parport(const char *fname)
{
	char nm[MAXPATHLEN];
	struct stat st;
	unsigned char c;
	int fd, m;

	int retv = build_full_device_path(nm, sizeof (nm), fname);
	if (0 != retv) {
		log_message(LOG_ERR, "Can't build path of lp device from [%s]: %s", fname, strerror(-retv));
		return (-1);
	}
	if ((fd = open(nm, O_RDWR | O_NONBLOCK)) == -1) {
		log_message(LOG_ERR, "open() failed for lp device [%s]: %s", nm, strerror(errno)); // TODO 2023.04.25: change this to NOTICE
		return (-1);
	}
	if (fstat(fd, &st) == -1) {
		log_message(LOG_ERR, "fstat() failed for lp device [%s]: %s", nm, strerror(errno));
		goto out;
	}
	if ((st.st_mode & S_IFMT) != S_IFCHR) {
		log_message(LOG_ERR, "lp device [%s] is not character device", nm);
		goto out;
	}
	if (ioctl(fd, PPISSTATUS, &c) == -1) {
		log_message(LOG_ERR, "ioctl(PPISSTATUS) failed for lp device [%s]: %s", nm, strerror(errno));
		goto out;
	}
	return (fd);
out:
	close(fd);
	return (-1);
}

#else                                  /* Fallback (nothing) */

int dev_get_parport(const char *fname)
{
	return (-1);
}

#endif





/* Linux wrapper around PPFCONTROL */
#ifdef HAVE_LINUX_PPDEV_H
static void
parport_control (int fd, unsigned char controlbits, int values)
{
	struct ppdev_frob_struct frob;
	frob.mask = controlbits;
	frob.val = values;

	if (ioctl (fd, PPFCONTROL, &frob) == -1)
	{
		cwdaemon_errmsg("Parallel port PPFCONTROL");
		exit (1);
	}
}
#endif

/* FreeBSD wrapper around PPISCTRL */
#ifdef HAVE_DEV_PPBUS_PPI_H
static void
parport_control (int fd, unsigned char controlbits, int values)
{
	unsigned char val;

	if (ioctl (fd, PPIGCTRL, &val) == -1)
	{
		cwdaemon_errmsg("Parallel port PPIGCTRL");
		exit (1);
	}

	val &= ~controlbits;
	val |= values;

	if (ioctl (fd, PPISCTRL, &val) == -1)
	{
		cwdaemon_errmsg("Parallel port PPISCTRL");
		exit (1);
	}
}
#endif

/* Linux wrapper around PPWDATA */
#ifdef HAVE_LINUX_PPDEV_H
static void
parport_write_data (int fd, unsigned char data)
{
	if (ioctl (fd, PPWDATA, &data) == -1)
	{
		cwdaemon_errmsg("Parallel port PPWDATA");
		exit (1);
	}
}
#endif

/* FreeBSD wrapper around PPISDATA */
#ifdef HAVE_DEV_PPBUS_PPI_H
static void
parport_write_data (int fd, unsigned char data)
{
	if (ioctl (fd, PPISDATA, &data) == -1)
	{
		cwdaemon_errmsg("Parallel port PPISDATA");
		exit (1);
	}
}
#endif

/* Linux wrapper around PPRSTATUS, reading the status port */
#ifdef HAVE_LINUX_PPDEV_H
static unsigned char
parport_read_data (int fd)
{
	unsigned char data = 0;
	if (ioctl (fd, PPRSTATUS, &data) == -1)
	{
		cwdaemon_errmsg("Parallel port PPRSTATUS");
		exit (1);
	}
	return data;
}
#endif

/* FreeBSD wrapper around PPISSTATUS, reading the status port */
#ifdef HAVE_DEV_PPBUS_PPI_H
static unsigned char
parport_read_data (int fd)
{
	unsigned char data = 0;
	if (ioctl (fd, PPISSTATUS, &data) == -1)
	{
		cwdaemon_errmsg("Parallel port PPISSTATUS");
		exit (1);
	}
	return data;
}
#endif

/* open port and setup ppdev */
int
lp_init (cwdevice * dev, int fd)
{
#ifdef HAVE_LINUX_PPDEV_H
	int mode = PARPORT_MODE_PCSPP;

	if (ioctl (fd, PPSETMODE, &mode) == -1)
	{
		cwdaemon_errmsg("Setting parallel port mode");
		close (fd);
		exit (1);
	}

	if (ioctl (fd, PPEXCL, NULL) == -1)
	{
		cwdaemon_errmsg("Parallel port %s is already in use", dev->desc);
		close (fd);
		exit (1);
	}
	if (ioctl (fd, PPCLAIM, NULL) == -1)
	{
		cwdaemon_errmsg("Claiming parallel port %s", dev->desc);
		cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__, "HINT: did you unload the lp kernel module?");
		cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__, "HINT: perhaps there is another cwdaemon running?");
		close (fd);
		exit (1);
	}

	/* Enable CW & PTT - /STROBE bit (pin 1) */
	parport_control (fd, PARPORT_CONTROL_STROBE, PARPORT_CONTROL_STROBE);
#endif
#ifdef HAVE_DEV_PPBUS_PPI_H
	parport_control (fd, STROBE, STROBE);
#endif
	dev->fd = fd;
	dev->reset (dev);
	return 0;
}

/* release ppdev and close port */
int
lp_free (cwdevice * dev)
{
#ifdef HAVE_LINUX_PPDEV_H
	dev->reset (dev);

	/* Disable CW & PTT - /STROBE bit (pin 1) */
	parport_control (dev->fd, PARPORT_CONTROL_STROBE, 0);

	ioctl (dev->fd, PPRELEASE);
#endif
#ifdef HAVE_DEV_PPBUS_PPI_H
	/* Disable CW & PTT - /STROBE bit (pin 1) */
	parport_control (dev->fd, STROBE, 0);
#endif
	close (dev->fd);
	return 0;
}

/* set to a known state */
int
lp_reset (cwdevice * dev)
{
#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
	lp_cw (dev, 0);
	lp_ptt (dev, 0);
	lp_ssbway (dev, 0);
	lp_switchband (dev, 0);
#endif
	return 0;
}

/* CW keying - /SELECT bit (pin 17) */
int
lp_cw (cwdevice * dev, int onoff)
{
#ifdef HAVE_LINUX_PPDEV_H
	if (onoff == 1) {
		parport_control (dev->fd, PARPORT_CONTROL_SELECT, 0);
	} else {
		parport_control (dev->fd, PARPORT_CONTROL_SELECT,
				PARPORT_CONTROL_SELECT);
	}
#endif
#ifdef HAVE_DEV_PPBUS_PPI_H
	if (onoff == 1) {
		parport_control (dev->fd, SELECTIN, 0);
	} else {
		parport_control (dev->fd, SELECTIN, SELECTIN);
	}
#endif
	return 0;
}

/* SSB PTT keying - /INIT bit (pin 16) (inverted) */
int
lp_ptt (cwdevice * dev, int onoff)
{
#ifdef HAVE_LINUX_PPDEV_H
	if (onoff == 1) {
		parport_control (dev->fd, PARPORT_CONTROL_INIT,
				PARPORT_CONTROL_INIT);
	} else {
		parport_control (dev->fd, PARPORT_CONTROL_INIT, 0);
	}
#endif
#ifdef HAVE_DEV_PPBUS_PPI_H
	if (onoff == 1) {
		parport_control (dev->fd, nINIT,
				nINIT);
	} else {
		parport_control (dev->fd, nINIT, 0);
	}
#endif
	return 0;
}

/* Foot switch reading / pin 15/bit 3 */
int
lp_footswitch (cwdevice * dev)
{
	unsigned char footswitch = 0xff;
	/* we check for bit 3 low so FF is better then 0 */
#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
	footswitch = parport_read_data (dev->fd);
	/* returns decimal 8 if pin 15 is high */
#endif
	return (int)((footswitch & 0x08) >> 3);
	/* bit 0=1 footswitch up, bit 0=0 footswitch down*/
}

/* SSB way from mic/soundcard - AUTOLF bit (pin 14) */
int
lp_ssbway (cwdevice * dev, int onoff)
{
#ifdef HAVE_LINUX_PPDEV_H
	if (onoff == 1)	{   /* soundcard */
		parport_control (dev->fd, PARPORT_CONTROL_AUTOFD,
				PARPORT_CONTROL_AUTOFD);
	} else {            /* microphone */
		parport_control (dev->fd, PARPORT_CONTROL_AUTOFD, 0);
	}
#endif
#ifdef HAVE_DEV_PPBUS_PPI_H
	if (onoff == 1)	{   /* soundcard */
		parport_control (dev->fd, AUTOFEED, AUTOFEED);
	} else {            /* microphone */
		parport_control (dev->fd, AUTOFEED, 0);
	}
#endif
	return 0;
}

/* Bandswitch output on pins 9 (MSB), 8, 7, and 2 (LSB). */
int
lp_switchband (cwdevice * dev, unsigned char bitpattern)
{
#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
	parport_write_data (dev->fd, bitpattern);
#endif
	return 0 ;
}
