/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *                      and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2023 Kamil Ignacak <acerion@wp.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#include <stdbool.h>




#ifndef OFF
# define OFF 0
#endif
#ifndef ON
# define ON 1
#endif
#define MICROPHONE 0
#define SOUNDCARD 1


/** ASCII character ESC (escape). */
#define ASCII_ESC 27


/* Notice that the range accepted by cwdaemon is different than that
   accepted by libcw. */
#define CWDAEMON_MORSE_WEIGHTING_DEFAULT        0
#define CWDAEMON_MORSE_WEIGHTING_MIN         (-50)
#define CWDAEMON_MORSE_WEIGHTING_MAX           50

#define CWDAEMON_MORSE_SPEED_DEFAULT           24 /* [wpm] */
#define CWDAEMON_MORSE_TONE_DEFAULT           800 /* [Hz] */
#define CWDAEMON_MORSE_VOLUME_DEFAULT          70 /* [%] */

/** Minimal acceptable number of port on which cdwaemon can listen. */
#define CWDAEMON_NETWORK_PORT_MIN           1024
/** Maximal acceptable number of port on which cdwaemon can listen. */
#define CWDAEMON_NETWORK_PORT_MAX          65535
/** Default number of port on which cwdaemon listens. */
#define CWDAEMON_NETWORK_PORT_DEFAULT       6789

/* TODO: why the limitation to 50 ms? Is it enough? */
#define CWDAEMON_PTT_DELAY_DEFAULT              0 /* [ms] */
#define CWDAEMON_PTT_DELAY_MIN                  0 /* [ms] */
#define CWDAEMON_PTT_DELAY_MAX                 50 /* [ms] */




/*
  Each message sent as a reply to a request is longer than the request by one
  byte. The replies are sent in response to either caret request or "esc
  reply" request.

  Caret request:       "ABCDE^"              -> 6 bytes (5 chars + caret)
  Reply:               "ABCDE\r\n            -> 7 bytes (5 chars + '\r' + '\n')

  esc reply request:   "\033hABCDE"          -> 7 bytes (ESC + code + 5 chars)
  Reply:                   "hABCDE\r\n"      -> 8 bytes (code + 5 chars + '\r' + '\n')

  In order to correctly store and send replies, the buffer for the replies
  must be larger by one byte than the buffer for requests.

  The socket send or receive code in cwdaemon doesn't treat the data in
  requests or replies as C string, so the size constants defined below don't
  include space for terminating NUL.
*/
#define CWDAEMON_REQUEST_SIZE_MAX                                256 /**< Size of cwdaemon's buffer for a request. */
#define CWDAEMON_REPLY_SIZE_MAX      (CWDAEMON_REQUEST_SIZE_MAX + 1) /**< Size of cwdaemon's buffer for a reply. */




/*
  Codes of Escape requests supported by cwdaemon.

  Values of the defines are treated by cwdaemon in case-sensitive way.
*/
#define CWDAEMON_ESC_REQUEST_RESET        '0' /**< ``'0'`` character == 0x30; reset some of parameters of cwdaemon. */
#define CWDAEMON_ESC_REQUEST_SPEED        '2' /**< ``'2'`` character == 0x32; set Morse speed [wpm]. */
#define CWDAEMON_ESC_REQUEST_TONE         '3' /**< ``'3'`` character == 0x33; set tone/frequency [Hz]. */
#define CWDAEMON_ESC_REQUEST_ABORT        '4' /**< ``'4'`` character == 0x34; abort currently sent message. */
#define CWDAEMON_ESC_REQUEST_EXIT         '5' /**< ``'5'`` character == 0x35; tell cwdaemon process to exit cleanly. Formerly known as STOP. */
#define CWDAEMON_ESC_REQUEST_WORD_MODE    '6' /**< ``'6'`` character == 0x36; enter or leave word mode (uninterruptible mode). */
#define CWDAEMON_ESC_REQUEST_WEIGHTING    '7' /**< ``'7'`` character == 0x37; set weighting of Morse code Dits and Dashes. */
#define CWDAEMON_ESC_REQUEST_CWDEVICE     '8' /**< ``'8'`` character == 0x38; use hardware keying device (cw device) specified by device name. Formerly known as DEVICE. */
#define CWDAEMON_ESC_REQUEST_PORT         '9' /**< ``'9'`` character == 0x39; set network port on which cwdaemon is listening. Obsolete. Formerly known as ADDRESS. */
#define CWDAEMON_ESC_REQUEST_PTT_STATE    'a' /**< ``'a'`` character == 0x61; set state of PTT pin. */
#define CWDAEMON_ESC_REQUEST_SSB_WAY      'b' /**< ``'b'`` character == 0x62; set pin 14 on lpt (set SSB way). Formerly known as SET14. */
#define CWDAEMON_ESC_REQUEST_TUNE         'c' /**< ``'c'`` character == 0x63; tune (send continuous wave) for a given number of seconds. */
#define CWDAEMON_ESC_REQUEST_TX_DELAY     'd' /**< ``'d'`` character == 0x64; set TX delay / PTT delay / Turn On Delay [ms]. Formerly known as TOD (Turn On Delay). */
#define CWDAEMON_ESC_REQUEST_BAND_SWITCH  'e' /**< ``'e'`` character == 0x65; set band switch output pins 2,7,8,9 on lpt. Formerly known as SWITCH. */
#define CWDAEMON_ESC_REQUEST_SOUND_SYSTEM 'f' /**< ``'f'`` character == 0x66; set sound system (Null/OSS/ALSA/PulseAudio). Formerly known as SDEVICE. */
#define CWDAEMON_ESC_REQUEST_VOLUME       'g' /**< ``'g'`` character == 0x67; set volume of sound [%]. */
#define CWDAEMON_ESC_REQUEST_REPLY        'h' /**< ``'h'`` character == 0x68; specify reply to be sent by cwdaemon after playing text. */




typedef struct {
	int log_threshold;
} options_t;




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

	/** UDP port the server listens on. */
	in_port_t network_port;

	/* cwdaemon usually receives requests from client, but on occasions
	   it needs to send a reply back. This is why in addition to
	   request_* we also have reply_* */
	struct sockaddr_in request_addr;
	socklen_t          request_addrlen;
	struct sockaddr_in reply_addr;
	socklen_t          reply_addrlen;
} cwdaemon_t;




#endif /* CWDAEMON_H */

