/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 -2005 Joop Stakenborg <pg4i@amsat.org>
 *                       and many authors, see the AUTHORS file.
 *
 * This program is free oftware; you can redistribute it and/or modify
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _CWDAEMON_H
#define _CWDAEMON_H

#ifndef OFF
# define OFF 0
#endif
#ifndef ON
# define ON 1
#endif
#define MICROPHONE 0
#define SOUNDCARD 1

#define MAX_DEVICE 20

typedef struct cwdev_s {
	int (*init) (struct cwdev_s *, int fd);
	int (*free) (struct cwdev_s *);
	int (*reset) (struct cwdev_s *);
	int (*cw) (struct cwdev_s *, int onoff);
	int (*ptt) (struct cwdev_s *, int onoff);
	int (*ssbway) (struct cwdev_s *, int onoff);
	int (*switchband) (struct cwdev_s *, unsigned char bandswitch);
	int (*footswitch) (struct cwdev_s *);
	int fd;
	char *desc; /* "parport0", "ttyS0", "null" - name of device used for keying. */
}
cwdevice;

void cwdaemon_errmsg(const char *info, ...);
void cwdaemon_debug(int level, const char *info, ...);

int dev_is_tty(const char *fname);
int dev_is_null(const char *fname);
int dev_is_parport(const char *fname);

#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
int lp_init (cwdevice * dev, int fd);
int lp_free (cwdevice * dev);
int lp_reset (cwdevice * dev);
int lp_cw (cwdevice * dev, int onoff);
int lp_ptt (cwdevice * dev, int onoff);
int lp_ssbway (cwdevice * dev, int onoff);
int lp_switchband (cwdevice * dev, unsigned char bandswitch);
int lp_footswitch (cwdevice * dev);
#endif

int ttys_init (cwdevice * dev, int fd);
int ttys_free (cwdevice * dev);
int ttys_reset (cwdevice * dev);
int ttys_cw (cwdevice * dev, int onoff);
int ttys_ptt (cwdevice * dev, int onoff);

int null_init (cwdevice * dev, int fd);
int null_free (cwdevice * dev);
int null_reset (cwdevice * dev);
int null_cw (cwdevice * dev, int onoff);
int null_ptt (cwdevice * dev, int onoff);

#endif /* _CWDAEMON_H */