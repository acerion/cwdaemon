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

#ifndef CWDAEMON_H
#define CWDAEMON_H




#include "config.h"




#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_NETDB
# include <netdb.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#include <stdbool.h>

#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
#include <lp.h>
#endif




#ifndef OFF
# define OFF 0
#endif
#ifndef ON
# define ON 1
#endif
#define MICROPHONE 0
#define SOUNDCARD 1

#define MAX_DEVICE 20

/* Notice that the range accepted by cwdaemon is different than that
   accepted by libcw. */
#define CWDAEMON_MORSE_WEIGHTING_DEFAULT        0
#define CWDAEMON_MORSE_WEIGHTING_MIN         (-50)
#define CWDAEMON_MORSE_WEIGHTING_MAX           50

#define CWDAEMON_MORSE_SPEED_DEFAULT           24 /* [wpm] */
#define CWDAEMON_MORSE_TONE_DEFAULT           800 /* [Hz] */
#define CWDAEMON_MORSE_VOLUME_DEFAULT          70 /* [%] */

#define CWDAEMON_NETWORK_PORT_DEFAULT                  6789

/* TODO: why the limitation to 50 ms? Is it enough? */
#define CWDAEMON_PTT_DELAY_DEFAULT              0 /* [ms] */
#define CWDAEMON_PTT_DELAY_MIN                  0 /* [ms] */
#define CWDAEMON_PTT_DELAY_MAX                 50 /* [ms] */




typedef struct cwdev_s {
	int (*init) (struct cwdev_s *, int fd);
	int (*free) (struct cwdev_s *);
	int (*reset) (struct cwdev_s *);
	int (*cw) (struct cwdev_s *, int onoff);
	int (*ptt) (struct cwdev_s *, int onoff);
	int (*ssbway) (struct cwdev_s *, int onoff);
	int (*switchband) (struct cwdev_s *, unsigned char bandswitch);
	int (*footswitch) (struct cwdev_s *);
	bool (*optparse) (struct cwdev_s *, const char *);
	bool (*optvalidate) (struct cwdev_s *);
	void *cookie; /* can be used by optparse */
	int fd;
	char *desc; /* "parport0", "ttyS0", "null" - name of device used for keying. */
}
cwdevice;


int dev_get_tty(const char *fname);
int dev_get_null(const char *fname);
int dev_get_parport(const char *fname);

int ttys_init (cwdevice * dev, int fd);
int ttys_free (cwdevice * dev);
int ttys_reset (cwdevice * dev);
int ttys_cw (cwdevice * dev, int onoff);
int ttys_ptt (cwdevice * dev, int onoff);
bool ttys_optparse (cwdevice * dev, const char * option);
bool ttys_optvalidate (cwdevice * dev);

int null_init (cwdevice * dev, int fd);
int null_free (cwdevice * dev);
int null_reset (cwdevice * dev);
int null_cw (cwdevice * dev, int onoff);
int null_ptt (cwdevice * dev, int onoff);




/**
   Instance of a cwdaemon
*/
typedef struct cwdaemon_t {
	int socket_descriptor;

	/* cwdaemon usually receives requests from client, but on occasions
	   it needs to send a reply back. This is why in addition to
	   request_* we also have reply_* */
	struct sockaddr_in request_addr;
	socklen_t          request_addrlen;
	struct sockaddr_in reply_addr;
	socklen_t          reply_addrlen;
} cwdaemon_t;




#endif /* CWDAEMON_H */

