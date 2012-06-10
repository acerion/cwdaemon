/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 -2005 Joop Stakenborg <pg4i@amsat.org>
 *		       and many authors, see the AUTHORS file.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

# if HAVE_STDIO_H
# include <stdio.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#if STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#if HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if DHAVE_ARPA_INET
# include <arpa/inet.h>
#endif
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
#if HAVE_SYSLOG_H
# include <syslog.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#if HAVE_SIGNAL_H
# include <signal.h>
#endif
#if HAVE_STDARG_H
# include <stdarg.h>
#endif
#if HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif
#if (defined(__unix__) || defined(unix)) && !defined(USG)
# include <sys/param.h>
#endif
#include <limits.h>
#include <assert.h>

#include <libcw.h>
#include "cwdaemon.h"


/**
   \file cwdaemon.c

   cwdaemon exchanges data with client through messages. Most of
   messages are sent by client application to cwdaemon - those
   are called in this file "requests". On several occasions cwdaemon
   sends some data back to the client. Such messages are called
   "replies".

   message:
       - request
       - reply

   Maximal size of a message is constant: CWDAEMON_MESSAGE_SIZE_MAX
   Size of a message can be smaller than that.
*/




/* cwdaemon constants. */
#define CWDAEMON_DEFAULT_MORSE_SPEED           24 /* [wpm] */
#define CWDAEMON_MIN_MORSE_SPEED     CW_SPEED_MIN /* from libcw.h */
#define CWDAEMON_MAX_MORSE_SPEED     CW_SPEED_MAX /* from libcw.h */
#define CWDAEMON_DEFAULT_MORSE_TONE           800 /* [Hz] */
#define CWDAEMON_DEFAULT_MORSE_VOLUME          70 /* [%] */
#define CWDAEMON_DEFAULT_PTT_DELAY              0 /* [microseconds]; TODO: convert PTT delay values to ms, and convert to usecs only when passing to libcw and cwdaemon_udelay(). */
#define CWDAEMON_DEFAULT_MORSE_WEIGHTING        0 /* in range -50/+50 */

#define CWDAEMON_DEFAULT_NETWORK_PORT              6789
#define CWDAEMON_DEFAULT_AUDIO_SYSTEM  CW_AUDIO_CONSOLE /* Console buzzer, from libcw.h. */

#define CWDAEMON_USECS_PER_MSEC    1000 /* just to avoid magic numbers */
#define CWDAEMON_MESSAGE_SIZE_MAX   256


/* Default values of parameters, may be modified only through
   commandline arguments passed to cwdaemon.
   These values are used when resetting libcw and cwdaemon to
   initial state. */
static int default_morse_speed  = CWDAEMON_DEFAULT_MORSE_SPEED;
static int default_morse_tone   = CWDAEMON_DEFAULT_MORSE_TONE;    /* TODO: add command line option for initial tone? */
static int default_morse_volume = CWDAEMON_DEFAULT_MORSE_VOLUME;
static int default_ptt_delay    = CWDAEMON_DEFAULT_PTT_DELAY;
static int default_audio_system = CWDAEMON_DEFAULT_AUDIO_SYSTEM;
static int default_weighting    = CWDAEMON_DEFAULT_MORSE_WEIGHTING;


/* Actual values of parameters, used to control ongoing operation of
   cwdaemon+libcw. These values can be modified through requests
   received from socket in cwdaemon_receive(). */
static int current_morse_speed  = CWDAEMON_DEFAULT_MORSE_SPEED;
static int current_morse_tone   = CWDAEMON_DEFAULT_MORSE_TONE;
static int current_morse_volume = CWDAEMON_DEFAULT_MORSE_VOLUME;
static int current_ptt_delay    = CWDAEMON_DEFAULT_PTT_DELAY;
static int current_audio_system = CWDAEMON_DEFAULT_AUDIO_SYSTEM;



/* Network variables. */
/* cwdaemon usually receives requests from client, but on occasions
   it needs to send a reply back. This is why in addition to
   request_* we also have reply_* */
int socket_descriptor;
static int port = CWDAEMON_DEFAULT_NETWORK_PORT;  /* default UDP port we listen on */

struct sockaddr_in request_addr;
socklen_t          request_addrlen;

struct sockaddr_in reply_addr;
socklen_t          reply_addrlen;

char reply_buffer[CWDAEMON_MESSAGE_SIZE_MAX];



/* various variables */
static int wordmode = 0;               /* start in character mode */
static int forking = 1;                /* we fork by default */
static int debuglevel = 0;             /* only debug when not forking */
static int bandswitch;
static int priority = 0;
static int async_abort = 0;
static int inactivity_seconds = 9999;  /* inactive since nnn seconds */


/* flags for different states */
static unsigned char ptt_flag = 0;	/* flag for PTT state/behaviour */
/* PTT on while sending, delay performed, switch PTT off when
   libcw's queue of characters becomes empty. */
#define PTT_ACTIVE_AUTO		0x01
/* PTT manually activated, switch off manually. */
#define PTT_ACTIVE_MANUAL	0x02
/* We must return an echo when libcw's queue of characters becomes empty. */
#define PTT_ACTIVE_ECHO		0x04

void cwdaemon_set_ptt_on(char *msg);
void cwdaemon_set_ptt_off(char *msg);

void cwdaemon_play_request(char *request);

void cwdaemon_tune(int seconds);
void cwdaemon_keyingevent(void *arg, int keystate);
void cwdaemon_prepare_reply(char *reply, const char *request, size_t n);
void cwdaemon_tone_queue_low_callback(void *arg);

ssize_t cwdaemon_sendto(const char *reply);
int     cwdaemon_recvfrom(char *request, int n);
int     cwdaemon_receive(void);
int     cwdaemon_handle_escaped_request(char *request);

void cwdaemon_reset_almost_all(void);
void cwdaemon_udelay(unsigned long us);

int  cwdaemon_open_libcw_output(int audio_system);
void cwdaemon_close_libcw_output(void);
void cwdaemon_reset_libcw_output(void);

void cwdaemon_parse_command_line(int argc, char *argv[]);
void cwdaemon_print_help(void);


struct timeval now, end, left;	/* PTT timers */


#define MAXMORSE 4000
static char request_queue[MAXMORSE];

char *keydev;

cwdevice cwdevice_ttys = {
	init:ttys_init,
	free:ttys_free,
	reset:ttys_reset,
	cw:ttys_cw,
	ptt:ttys_ptt
#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
	,ssbway:NULL,
	switchband:NULL,
	footswitch:NULL
#endif
};

cwdevice cwdevice_null = {
   init:null_init,
   free:null_free,
   reset:null_reset,
   cw:null_cw,
   ptt:null_ptt
#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
   ,ssbway:NULL,
   switchband:NULL,
   footswitch:NULL
#endif
};

#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
cwdevice cwdevice_lp = {
	init:lp_init,
	free:lp_free,
	reset:lp_reset,
	cw:lp_cw,
	ptt:lp_ptt,
	ssbway:lp_ssbway,
	switchband:lp_switchband,
	footswitch:lp_footswitch
};
#endif
cwdevice *cwdev;





/* catch ^C when running in foreground */
static RETSIGTYPE
catchint (int signal)
{
	cwdev->free (cwdev);
	printf ("%s: Exiting\n", PACKAGE);
	exit (0);
}

/* print error message to the console or syslog if we are forked */
void
errmsg (char *info, ...)
{
	va_list ap;
	char s[1025];

	va_start (ap, info);
	vsnprintf (s, 1024, info, ap);
	va_end (ap);

	if (forking)
		syslog (LOG_ERR, "%s\n", s);
	else
		printf ("%s: %s failed: %s\n", PACKAGE, s, strerror (errno));
}


/* FIXME: come up with nice symbolic names for debug
   levels - don't use magic numbers. */
void cwdaemon_debug(int level, char *info, ...)
{
	if (level <= debuglevel) {
		va_list ap;
		char s[1024 + 1];

		va_start(ap, info);
		vsnprintf(s, 1024, info, ap);
		va_end(ap);
		printf("%s: %s \n", PACKAGE, s);
	}

	return;
}





/* delay in microseconds */
void cwdaemon_udelay(unsigned long us)
{
	struct timespec sleeptime, time_remainder;

	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = us * 1000;

	if (nanosleep(&sleeptime, &time_remainder) == -1) {
		if (errno == EINTR) {
			nanosleep(&time_remainder, NULL);
		} else {
			errmsg("Nanosleep");
		}
	}

	return;
}





/* band switch function,  pin 2, 7, 8, 9 */
#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
static void
set_switch (unsigned int bandswitch)
{
	unsigned int lp_switchbyte = 0;

	lp_switchbyte = (bandswitch & 0x01) | ((bandswitch & 0x0e) * 16);
	if (cwdev->switchband)
	{
		cwdev->switchband (cwdev, lp_switchbyte);
		cwdaemon_debug(1, "Set bandswitch to %x", bandswitch);
	}
	else
		cwdaemon_debug(1, "Bandswitch output not implemented");
}
#endif





/**
   \brief Switch PTT on

   \param msg - debug message displayed when performing the switching
*/
void cwdaemon_set_ptt_on(char *msg)
{
	if (current_ptt_delay && !(ptt_flag & PTT_ACTIVE_AUTO)) {
		cwdev->ptt(cwdev, ON);
		cwdaemon_debug(1, msg);
		int rv = cw_queue_tone(current_ptt_delay * 20, 0);	/* try to 'enqueue' delay */
		if (rv == CW_FAILURE) {			        /* old libcw may reject freq=0 */
			cwdaemon_debug(1, "cw_queue_tone failed: rv=%d errno=%s, using udelay instead",
				       rv, strerror(errno));
			cwdaemon_udelay(current_ptt_delay);
		}
		ptt_flag |= PTT_ACTIVE_AUTO;
	}

	return;
}





/**
   \brief Switch PTT off

   \param msg - debug message displayed when performing the switching
*/
void cwdaemon_set_ptt_off(char *msg)
{
	cwdev->ptt(cwdev, OFF);
	ptt_flag = 0;
	cwdaemon_debug(1, msg);

	return;
}





/**
   \brief Tune for a number of seconds

   \param seconds - time of tuning
*/
void cwdaemon_tune(int seconds)
{
	if (seconds > 0) {
		cw_flush_tone_queue();
		cwdaemon_set_ptt_on("PTT (TUNE) on");

		/* make it similar to normal CW, allowing interrupt */
		int i = 0;
		for (i = 0; i < seconds; i++) {
			cw_queue_tone(1000000, current_morse_tone);
		}

		cw_send_character('e');	/* append minimal tone to return to normal flow */
	}

	return;
}





/**
   \brief Reset some initial parameters of cwdaemon and libcw
*/
void cwdaemon_reset_almost_all(void)
{
	current_morse_speed = default_morse_speed;
	current_morse_tone = default_morse_tone;
	current_morse_volume = default_morse_volume;
	current_audio_system = default_audio_system;
	current_ptt_delay = default_ptt_delay;

	cwdaemon_reset_libcw_output();

	return;
}





/**
   \brief Open audio sink using libcw

   \param audio_system - audio system to be used by libcw

   \return -1 on failure
   \return 0 otherwise
*/
int cwdaemon_open_libcw_output(int audio_system)
{
	int rv = cw_generator_new(audio_system, NULL);
	if (audio_system == CW_AUDIO_OSS && rv == CW_FAILURE) {
		/* When reopening libcw output, previous audio system may
		   block audio device for a short period of time after the
		   output has been closed. In such a situation OSS may fail
		   to open audio device. Let's give it some time. */
		int i = 0;
		for (i = 0; i < 5; i++) {
			sleep(4);
			rv = cw_generator_new(audio_system, NULL);
			if (rv == CW_SUCCESS) {
				break;
			}
		}
	}
	if (rv != CW_FAILURE) {
		rv = cw_generator_start();
		fprintf(stderr, "switching to audio %d: %d\n", audio_system, rv);
	} else {
		/* FIXME:
		   When cwdaemon failed to create a generator, and user
		   kills non-forked cwdaemon through Ctrl+C, there is
		   a memory protection error. */
		cwdaemon_debug(1, "failed to create generator with audio system %d\n", audio_system);
	}

	return rv == CW_FAILURE ? -1 : 0;
}





void cwdaemon_close_libcw_output(void)
{
	cw_generator_stop();
	cw_generator_delete();

	return;
}





void cwdaemon_reset_libcw_output(void)
{
	/* Delete old generator (if it exists). */
	cwdaemon_close_libcw_output();

	cwdaemon_debug(3, "Setting audio system=%d", current_audio_system); /* TODO: get human-readable label */
	cwdaemon_open_libcw_output(current_audio_system);

	/* FIXME: current_* or default_* ? */
	cw_set_frequency(current_morse_tone);
	cw_set_send_speed(current_morse_speed);
	cw_set_volume(current_morse_volume);
	cw_set_gap(0);
	cw_set_weighting (default_weighting * 0.6 + 50);

	return;
}





/* properly parse a 'long' integer */
static int
get_long(const char *buf, long *lvp)
{
	char *ep;
	long lv;

	errno = 0;
	lv = strtol(buf, &ep, 10);
	if (buf[0] == '\0' || *ep != '\0')
		return (-1);
	if (errno == ERANGE && (lv == LONG_MAX || lv == LONG_MIN))
		return (-1);
	*lvp = lv;
	return (0);
}





/**
   \brief Prepare reply for the caller

   Fill \p reply buffer with data from given \p request, prepare some
   other variables for sending reply to the client.

   Text of the reply is usually defined by caller, i.e. it is sent by client
   to cwdaemon and marked by the client as text to be used in reply.

   The reply should be sent back to client as soon as cwdaemon
   finishes processing/playing received request.

   There are two different procedures for recognizing what should be sent
   back as reply and when:
   \li received request ending with '^' character: the text of the request
       should be played, but it also should be used as a reply. The reply
       should be sent back to the client as soon as cwdaemon finishes playing
       text of request.
       Think about it as a form of "confirm by copy".
   \li received request starting with "<ESC>h" escape code: the text of
       request should be sent back to the client after playing text of *next*
       request. So there are two requests sent by client to cwdaemon:
       first defines reply, and the second defines text to be played.
       First should be echoed back (but not played), second should be played.
*/
void cwdaemon_prepare_reply(char *reply, const char *request, size_t n)
{
	memcpy(&reply_addr, &request_addr, sizeof(reply_addr)); /* Remember sender. */
	reply_addrlen = request_addrlen;

	strncpy(reply, request, n);
	cwdaemon_debug(2, "request='%s', reply='%s'", request, reply);
	cwdaemon_debug(1, "now waiting for end of transmission before echo");

	ptt_flag |= PTT_ACTIVE_ECHO;				/* wait for tone queue to become empty, then echo back */

	return;
}





/**
   \brief Wrapper around sendto()

   Wrapper around sendto(), sending a \p reply to client.
   Client is specified by reply_* network variables.

   \param reply - reply to be sent

   \return -1 on failure
   \return number of characters sent on success
*/
ssize_t cwdaemon_sendto(const char *reply)
{
	size_t len = strlen(reply);
	/* TODO: Do we *really* need to end replies with CRLF? */
	assert(reply[len - 2] == '\r' && reply[len - 1] == '\n');

	ssize_t rv = sendto(socket_descriptor, reply, len, 0,
			    (struct sockaddr *) &reply_addr, reply_addrlen);

	if (rv == -1) {
		cwdaemon_debug(1, "sendto: %s\n", strerror(errno));
		return -1;
	} else {
		return rv;
	}
}





/**
   \brief Receive request sent through socket

   Received request is returned through \p request.
   Possible trailing '\r' and '\n' characters are stripped.
   Request is ended with '\0'.

   \param request - buffer for received request
   \param n - size of the buffer

   \return -2 if peer has performed an orderly shutdown
   \return -1 if an error occurred during call to recvfrom
   \return  0 if no request has been received
   \return length of received request otherwise
 */
int cwdaemon_recvfrom(char *request, int n)
{
	ssize_t recv_rc = recvfrom(socket_descriptor,
				   request,
				   n,
				   0, /* flags */
				   (struct sockaddr *) &request_addr,
				   &request_addrlen);

	if (recv_rc == -1) { /* No requests available? */

		if (errno == EAGAIN || errno == EWOULDBLOCK) { /* "a portable application should check for both possibilities" */
			/* Yup, no requests available from non-blocking socket. Good luck next time. */
			/* TODO: how much CPU time does it cost to loop waiting for a request?
			   Would it be reasonable to configure the socket as blocking?
			   How large is receive timeout? */

			return 0;
		} else {
			/* Some other error. May be a serious error. */
			errmsg("Recvfrom");
			return -1;
		}
	} else if (recv_rc == 0) {
		/* "peer has performed an orderly shutdown" */
		return -2;
	} else {
		; /* pass */
	}

	/* Remove CRLF if present. TCP buffer may end with '\n', so make
	   sure that every request is consistently ended with NUL.
	   Do it early, do it now. */
	int z;
	while (recv_rc > 0
	       && ( (z = request[recv_rc - 1]) == '\n' || z == '\r') ) {

		request[--recv_rc] = '\0';
	}

	return recv_rc;
}





/* watch the socket and if there is an escape character check what it is,
   otherwise play morse.

   \return 0 when an escape code has been received
   \return 0 when no request or an empty request has been received
   \return 1 when a text request has been played
*/
int cwdaemon_receive(void)
{
	/* We may treat request as printable string, so +1 for
	   ending NUL added somewhere below is necessary. */
	char request_buffer[CWDAEMON_MESSAGE_SIZE_MAX + 1];

	ssize_t recv_rc = cwdaemon_recvfrom(request_buffer, CWDAEMON_MESSAGE_SIZE_MAX);

	if (recv_rc == -2) {
		/* Sender has closed connection. */
		return 0;
	} else if (recv_rc == -1) {
		/* TODO: should we really exit?
		   Shouldn't we recover from the error? */
		exit(EXIT_FAILURE);
	} else if (recv_rc == 0) {
		cwdaemon_debug(2, "...recv_from (no data)");
		return 0;
	} else { /* TODO: remove 'else', re-indent */
		; /* pass */
	}

	request_buffer[recv_rc] = '\0';

	if (request_buffer[0] != 27) {
		/* No ESCAPE. All received data should be treated
		   as text to be sent using Morse code. */
		cwdaemon_debug(1, "Request = %s", request_buffer);
		if ((strlen(request_buffer) + strlen(request_queue)) <= MAXMORSE - 1) {
			/* Hmm, seems that 'request_queue' is a kind of
			   simple FIFO for text to be played. Would
			   a better FIFO be a good thing? */
			strcat(request_queue, request_buffer);
			cwdaemon_play_request(request_queue);
		} else {
			; /* TODO */
		}
		return 1;
	} else {
		return cwdaemon_handle_escaped_request(request_buffer);
	}
}





int cwdaemon_handle_escaped_request(char *request)
{
	long lv;

	/* Take action depending on Escape code. */
	switch ((int) request[1]) {
	case '0':	/* reset  all values */
		request_queue[0] = '\0';
		cwdaemon_reset_almost_all();
		wordmode = 0;
		async_abort = 0;
		cwdev->reset(cwdev);
		ptt_flag = 0;
		cwdaemon_debug(1, "Reset all values");
		break;
	case '2':
		/* Set speed of morse code, in words per minute. */
		if (get_long(request + 2, &lv)) {
			break;
		}

		if (lv >= CWDAEMON_MIN_MORSE_SPEED && lv <= CWDAEMON_MAX_MORSE_SPEED) {
			current_morse_speed = lv;
			cw_set_send_speed(current_morse_speed);
			cwdaemon_debug(1, "Speed: %d wpm", current_morse_speed);
		}

		break;
	case '3':
		/* Set tone (frequency) of morse code, in Hz. */
		if (get_long(request + 2, &lv)) {
			break;
		}

		if (lv > 0 && lv <= CW_FREQUENCY_MAX) {
			current_morse_tone = lv;
			cw_set_frequency(current_morse_tone);
			/* TODO: should we modify volume when modifying tone? This doesn't seem right. */
			cw_set_volume(default_morse_volume);
			cwdaemon_debug(1, "Tone: %s Hz, volume %d%", request + 2, default_morse_volume);

		} else if (lv == 0) {	/* sidetone off */
			current_morse_tone = 0;
			cw_set_volume(0);
			cwdaemon_debug(1, "Volume off");

		} else {
			;
		}

		break;
	case '4':
		/* Abort currently sent message. */
		if (wordmode) {
			cwdaemon_debug(1, "Ignoring Message abort request");
		} else {
			cwdaemon_debug(1, "Message abort");
			if (ptt_flag & PTT_ACTIVE_ECHO) {		/* if echo pending */
				cwdaemon_debug(1, "Echo 'break'");
				cwdaemon_sendto("break\r\n");
			}
			request_queue[0] = '\0';
			cw_flush_tone_queue();
			cw_wait_for_tone_queue();
			if (ptt_flag) {
				cwdaemon_set_ptt_off("PTT off");
			}
			ptt_flag &= 0;
		}
		break;
	case '5':
		/* Exit cwdaemon. */
		cwdev->free(cwdev);
		errno = 0;
		errmsg("Sender has told me to end the connection");
		exit(EXIT_SUCCESS);

	case '6':	/* set uninterruptable */
		request[0] = '\0';
		request_queue[0] = '\0';
		wordmode = 1;
		cwdaemon_debug(1, "Wordmode set");
		break;
	case '7':
		/* Set weighting of morse code dits and dashes.
		   Remember that cwdaemon uses values in range
		   -50/+50, but libcw accepts values in range
		   20/80. This is why you have the calculation
		   when calling cw_set_weighting(). */
		if (get_long(request + 2, &lv)) {
			break;
		}

		if ((lv > -51) && (lv < 51)) {
			cw_set_weighting(lv * 0.6 + 50);
			cwdaemon_debug(1, "Weight: %ld", lv);
		}
		break;
	case '8': {
		/* Set device type. */
		char *ndev = request + 2;
		int fd;
		cwdaemon_debug(1, "Device: %s", ndev);
		if ((fd = dev_is_tty(ndev)) != -1) {
			cwdev->free(cwdev);
			cwdev = &cwdevice_ttys;
			keydev = cwdev->desc = ndev;
			cwdev->init(cwdev, fd);
		}
#if defined(HAVE_LINUX_PPDEV_H) || defined(HAVE_DEV_PPBUS_PPI_H)
		else if ((fd = dev_is_parport(ndev)) != -1) {
			cwdev->free(cwdev);
			cwdev = &cwdevice_lp;
			keydev = cwdev->desc = ndev;
			cwdev->init(cwdev, fd);
		}
#endif
		else if ((fd = dev_is_null(ndev)) != -1) {
			cwdev->free(cwdev);
			cwdev = &cwdevice_null;
			keydev = cwdev->desc = ndev;
			cwdev->init(cwdev, fd);
		} else {
			cwdaemon_debug(1, "Unknown device");
		}
		break;
	}
	case '9':
		/* Base port number.
		   TODO: why this is obsolete? */
		cwdaemon_debug(1, "Obsolete control data '9'.");
		break;
	case 'a':	/* PTT keying on or off */
		if (get_long(request + 2, &lv)) {
			break;
		}

		if (lv) {
			cwdev->ptt (cwdev, ON);
			if (current_ptt_delay) {
				cwdaemon_set_ptt_on("PTT (manual, delay) on");
			} else {
				cwdaemon_debug(1, "PTT (manual, immediate) on");
			}
			ptt_flag |= PTT_ACTIVE_MANUAL;
		} else if (ptt_flag & PTT_ACTIVE_MANUAL) {	/* only if manually activated */
			ptt_flag &= ~PTT_ACTIVE_MANUAL;
			if (!(ptt_flag & !PTT_ACTIVE_AUTO)) {	/* no PTT modifiers */

				if (request_queue[0] == '\0'/* no new text in the meantime */
				    && cw_get_tone_queue_length() <= 1) {

					cwdaemon_set_ptt_off("PTT (manual, immediate) off");
				} else {
					/* still sending, cannot yet switch PTT off */
					ptt_flag |= PTT_ACTIVE_AUTO;	/* ensure auto-PTT active */
					cwdaemon_debug(1, "reverting from PTT (manual) to PTT (auto) now");
				}
			}
		}
		break;
	case 'b':	/* SSB way */
#if defined(HAVE_LINUX_PPDEV_H) || defined(HAVE_DEV_PPBUS_PPI_H)
		if (get_long(request + 2, &lv)) {
			break;
		}

		if (lv) {
			if (cwdev->ssbway) {
				cwdev->ssbway (cwdev, SOUNDCARD);
				cwdaemon_debug(1, "SSB way set to SOUNDCARD", PACKAGE);
			} else {
				cwdaemon_debug(1, "SSB way to SOUNDCARD unimplemented");
			}
		} else {
			if (cwdev->ssbway) {
				cwdev->ssbway (cwdev, MICROPHONE);
				cwdaemon_debug(1, "SSB way set to MIC");
			} else {
				cwdaemon_debug(1, "SSB way to MICROPHONE unimplemented");
			}
		}
#else
		cwdaemon_debug(1, "Unavailable");
#endif
		break;
	case 'c':
		/* Tune for a number of seconds */
		if (get_long(request + 2, &lv)) {
			break;
		}

		if (lv <= 10) {
			cwdaemon_tune(lv);
		}
		break;
	case 'd':
		/* Set PTT delay (TOD, Turn On Delay).
		   The value is milliseconds. */
		if (get_long(request + 2, &lv)) {
			break;
		}

		if (lv >= 0 && lv < 51) {
			current_ptt_delay = lv * CWDAEMON_USECS_PER_MSEC;
		} else {
			current_ptt_delay = 50000;
		}

		cwdaemon_debug(1, "PTT delay(TOD): %d ms", current_ptt_delay / CWDAEMON_USECS_PER_MSEC);

		if (current_ptt_delay == 0) {
			cwdaemon_set_ptt_off("ensure PTT off");
		}
		break;
	case 'e':	/* set bandswitch output on parport bits 2(lsb),7,8,9(msb) */
#if defined(HAVE_LINUX_PPDEV_H) || defined(HAVE_DEV_PPBUS_PPI_H)
		if (get_long(request + 2, &lv))
			break;
		if (lv <= 15 && lv >= 0) {
			bandswitch = lv;
			set_switch(bandswitch);
		}
#else
		cwdaemon_debug(1, "Parallel port unavailable");
#endif
		break;
	case 'f': {
		/* Change sound system used by libcw. */
		int c = request[2];
		if (c == 'n') {
			current_audio_system = CW_AUDIO_NULL;
		} else if (c == 'c') {
			current_audio_system = CW_AUDIO_CONSOLE;
		} else if (c == 's') {
			current_audio_system = CW_AUDIO_SOUNDCARD;
		} else if (c == 'a') {
			current_audio_system = CW_AUDIO_ALSA;
		} else if (c == 'p') {
			current_audio_system = CW_AUDIO_PA;
		} else if (c == 'o') {
			current_audio_system = CW_AUDIO_OSS;
		} else {
			cwdaemon_debug(1, "Invalid sound system: %s", request + 2);
			break;
		}

		/* Handle valid request for changing sound system. */
		cwdaemon_debug(1, "Sound device: %s", request + 2);
		cwdaemon_close_libcw_output();
		cwdaemon_open_libcw_output(current_audio_system);
		break;
	}
	case 'g':
		/* Set volume of sound, in percents. */
		if (get_long(request + 2, &lv)) {
			break;
		}

		if (lv >= 0 && lv <= 100) {
			current_morse_volume = lv;
			cw_set_volume(current_morse_volume);
		}
		break;
	case 'h':
		/* Data after '<ESC>h' is a text to be used as reply.
		   It shouldn't be echoed back to client
		   immediately.
		   Instead, cwdaemon should wait for another
		   request (I assume that it will be a
		   regular text to be played), play
		   it, and then send prepared reply back to
		   the client.
		   So this is a reply with delay. */
		cwdaemon_prepare_reply(reply_buffer, request + 2, strlen(request) - 2);
		/* wait for queue-empty callback */
		break;
	} /* switch ((int) request[1]) */

	return 0;
}





/**
   \brief Process received request, play relevant characters

   Check every character in given request, act upon markers
   for speed increase or decrease, and play other characters.

   Function modifies contents of buffer \p request.

   \param request - request to be processed
*/
void cwdaemon_play_request(char *request)
{
	char *x = request;

	while (*x) {
		switch ((int) *x) {
		case '+':
		case '-':
			/* speed in- & decrease */
			/* TODO: it seems that multiple '+' and '-' are allowed.
			   Is it described anywhere? */
			do {
				current_morse_speed += (*x == '+') ? 2 : -2;
				x++;
			} while (*x == '+' || *x == '-');

			if (current_morse_speed < CWDAEMON_MIN_MORSE_SPEED) {
				current_morse_speed = CWDAEMON_MIN_MORSE_SPEED;
			} else if (current_morse_speed > CWDAEMON_MAX_MORSE_SPEED) {
				current_morse_speed = CWDAEMON_MAX_MORSE_SPEED;
			} else {
				;
			}
			cw_set_send_speed(current_morse_speed);
			break;
		case '~':
			cw_set_gap(2); /* 2 dots time additional for the next char */
			x++;
			break;
		case '^':              /* Send echo to main program when CW playing is done. */
			*x = '\0';     /* Remove '^' and possible trailing garbage. */
			/* I'm guessing here that '^' can be found at
			   the end of request, and it means "echo text of current
			   request back to sender once you finish playing it". */
			cwdaemon_prepare_reply(reply_buffer, request, strlen(request) - 2);
			/* wait for queue-empty callback */
			break;
		case '*':
			*x = '+';
		default:
			cwdaemon_set_ptt_on("PTT (auto) on");
			cwdaemon_debug(1, "Morse = %c", *x);
			cw_send_character(*x);
			x++;
			if (cw_get_gap() == 2) {
				if (*x == '^') { /* TODO: ??? */
					x++;
				} else {
					cw_set_gap(0);
				}
			}
			break;
		}
	}

	/* All characters processed, mark buffer. */
	*request = '\0';

	return;
}





/**
   Function passed to libcw, will be called every time libcw notices
   change of a key state.
   When key is closed (dit or dah), \p keystate is 1.
   Otherwise \p keystate is 0.

   \param unused argument
   \param keystate - state of a key, as seen by libcw
*/
void cwdaemon_keyingevent(void *arg, int keystate)
{
	if (keystate == 1) {
		cwdev->cw(cwdev, ON);
	} else {
		cwdev->cw(cwdev, OFF);
	}

	inactivity_seconds = 0;

	cwdaemon_debug(3, "keying event %d", keystate);

	return;
}





/* Callback routine when tone queue is empty */
void cwdaemon_tone_queue_low_callback(void *arg)
{
	cwdaemon_debug(2, "entering-Q-empty ptt_flag=%02x", ptt_flag);
	if (ptt_flag == PTT_ACTIVE_AUTO &&		/* PTT on, w/o manual PTT or similar */
		request_queue[0] == '\0' &&			/* no new text in the meantime */
	    cw_get_tone_queue_length() <= 1) {

		cwdaemon_set_ptt_off("PTT (auto) off");

	} else if (ptt_flag & PTT_ACTIVE_ECHO) {        /* if waiting for echo */

		cwdaemon_debug(1, "Echo '%s'", reply_buffer);
		strcat(reply_buffer, "\r\n"); /* Ensure exactly one CRLF */
		cwdaemon_sendto(reply_buffer);
		reply_buffer[0] = '\0';

		ptt_flag &= ~PTT_ACTIVE_ECHO;

		/* wait a bit more since we expect to get more text to send */
		if (ptt_flag == PTT_ACTIVE_AUTO) {
			cw_queue_tone(1, 0); /* ensure Q-empty condition again */
			cw_queue_tone(1, 0); /* when trailing gap also 'sent' */
		}
	}

	return;
}





/* parse the command line and check for options, do some error checking */
void cwdaemon_parse_command_line(int argc, char *argv[])
{
	long lv;
	int p;

	while ((p = getopt(argc, argv, "d:hnp:P:s:t:v:Vw:x:")) != -1) {
		switch (p) {
		case ':':
		case '?':
		case 'h':
			cwdaemon_print_help();
			exit(EXIT_SUCCESS);
		case 'd':
			keydev = optarg;
			break;
		case 'n':
			if (forking) {
				printf("%s: Not forking...\n", PACKAGE);
				forking = 0;
			}
			debuglevel++;
			break;
		case 'p':
			if (get_long(optarg, &lv) || lv < 1024 || lv > 65536) {
				printf("%s: invalid port number - %s\n",
				       PACKAGE, optarg);
				exit(EXIT_FAILURE);
			}
			port = lv;
			break;
#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
		case 'P':
			if (get_long(optarg, &lv) || lv < -20 || lv > 20) {
				printf("%s: bad priority %s (should be between -20 and 20 inclusive)\n",
				       PACKAGE, optarg);
				exit(EXIT_FAILURE);
			}
			priority = lv;
			break;
#endif
		case 's':
			if (get_long(optarg, &lv) || lv < CWDAEMON_MIN_MORSE_SPEED || lv > CWDAEMON_MAX_MORSE_SPEED) {
				printf("%s: bad speed %s (should be between %d and %d inclusive)\n",
				       PACKAGE, optarg,
				       CWDAEMON_MIN_MORSE_SPEED, CWDAEMON_MAX_MORSE_SPEED);
				exit(EXIT_FAILURE);
			}
			default_morse_speed = lv;
			break;
		case 't':
			if (get_long(optarg, &lv) || lv < 0 || lv > 50) {
				printf("%s: bad PTT delay value %s (should be between 0 and 50 ms inclusive)\n",
				       PACKAGE, optarg);
				exit(EXIT_FAILURE);
			}
			default_ptt_delay = CWDAEMON_USECS_PER_MSEC * lv;
			break;
		case 'v':
			if (get_long(optarg, &lv) || lv < 0 || lv > 100) {
				printf("%s: bad volume %s (should be between 0 and 100 inclusive)\n",
				       PACKAGE, optarg);
				exit(EXIT_FAILURE);
			}
			default_morse_volume = lv;
			break;
		case 'V':
			printf("%s version %s\n", PACKAGE, VERSION);
			exit(EXIT_SUCCESS);
		case 'w':
			if (get_long(optarg, &lv) || lv < -50 || lv > 50) {
				printf("%s: bad weight %s (should be between -50 and 50 inclusive)\n",
				       PACKAGE, optarg);
				exit(EXIT_FAILURE);
			}
			default_weighting = lv;
			break;
		case 'x':
			if (!strncmp(optarg, "n", 1)) {
				default_audio_system = CW_AUDIO_NULL;
			} else if (!strncmp(optarg, "c", 1)) {
				default_audio_system = CW_AUDIO_CONSOLE;
			} else if (!strncmp(optarg, "s", 1)) {
				default_audio_system = CW_AUDIO_SOUNDCARD;
			} else if (!strncmp(optarg, "a", 1)) {
				default_audio_system = CW_AUDIO_ALSA;
			} else if (!strncmp(optarg, "p", 1)) {
				default_audio_system = CW_AUDIO_PA;
			} else if (!strncmp(optarg, "o", 1)) {
				default_audio_system = CW_AUDIO_OSS;
			} else {
				printf("Wrong sound device, use c(onsole), o(ss), a(lsa), p(ulseaudio), n(one - no audio), or s(oundcard - autoselect from OSS/ALSA/PulseAudio)\n");
				exit(EXIT_FAILURE);
			}
			break;
		} /* switch (p) */
	} /* while ((p = getopt()) */

	return;
}





void cwdaemon_print_help(void)
{
	printf("Usage: %s [option]...\n\n", PACKAGE);

	printf("-h\n");
	printf("        Display this help and exit\n");
	printf("-V\n");
	printf("        Output version information and exit\n");

	printf("-d <device>\n");
	printf("        Use a different device\n");
#if defined (HAVE_LINUX_PPDEV_H)
	printf("        (e.g. ttyS0,1,2, parport0,1, etc. default = parport0)\n");
#elif defined (HAVE_DEV_PPBUS_PPI_H)
	printf("        (e.g. ttyd0,1,2, ppi0,1, etc. default = ppi0)\n");
#else
#ifdef BSD
	printf("        (e.g. ttyd0,1,2, etc. default = ttyd0)\n");
#else
	printf("        (e.g. ttyS0,1,2, etc. default = ttyS0)\n");
#endif
#endif
	printf("        Use \"null\" for dummy device (no rig keying, no ssb keying, etc.).\n");

	printf("-n\n");
	printf("        Do not fork. Print debug information to stdout.\n");
	printf("-p <port>\n");
	printf("        Use a different UDP port number (> 1023, default = %d)\n", CWDAEMON_DEFAULT_NETWORK_PORT);
#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
	printf("-P <priority>\n");
	printf("        Set cwdaemon priority (-20 ... 20, default = 0)\n");
#endif
	printf("-s <speed>\n");
	printf("        Set morse speed (%d ... %d wpm, default = %d)\n",
	       CWDAEMON_MIN_MORSE_SPEED, CWDAEMON_MAX_MORSE_SPEED, CWDAEMON_DEFAULT_MORSE_SPEED);
	printf("-t <time>\n");
	printf("        Set PTT delay (0 ... 50 ms, default = 0)\n");
	printf("-v <volume>\n");
	printf("        Set volume for soundcard output\n");
	printf("-w <weight>\n");
	printf("        Set weighting (-50 ... 50, default = 0)\n");
	printf("-x <sound system>\n");
	printf("        Use a specific sound system:\n");
	printf("        c = console buzzer (default)\n");
	printf("        o = OSS\n");
	printf("        a = ALSA\n");
	printf("        p = PulseAudio\n");
	printf("        n = none (no audio)\n");
	printf("        s = soundcard (autoselect from OSS/ALSA/PulseAudio)\n");

	return;
}





/* main program: fork, open network connection and go into an endless loop
   waiting for something to happen on the UDP port */
int
main (int argc, char *argv[])
{
	pid_t pid, sid;
	int fd, fd_count, save_flags;

#if defined HAVE_LINUX_PPDEV_H
	keydev = "parport0";
#elif defined HAVE_DEV_PPBUS_PPI_H
	keydev = "ppi0";
#else
# if defined(__FreeBSD__)
	keydev = "ttyd0";
# elif defined(__OpenBSD__)
	keydev = "tty00";
# else
	keydev = "ttyS0";
# endif
#endif

	cwdaemon_parse_command_line(argc, argv);

	if (debuglevel > 3) {		/* debugging cwlib as well */
		/* FIXME: pass correct libcw debug flags. */
		cw_set_debug_flags((1 << (debuglevel - 3)) -1);
	}


	if ((fd = dev_is_tty(keydev)) != -1)
		cwdev = &cwdevice_ttys;
#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
	else if ((fd = dev_is_parport(keydev)) != -1)
	{
		cwdev = &cwdevice_lp;
		if (geteuid () != 0)
		{
			printf ("You must run this program as root\n");
			exit (1);
		}
	}
#endif
	else if ((fd = dev_is_null(keydev)) != -1)
		cwdev = &cwdevice_null;

	if (cwdev == NULL)
	{
		printf("%s: bad keyer device: %s\n", PACKAGE, keydev);
		exit(1);
	}
	cwdev->desc = keydev;

	cwdaemon_reset_almost_all();
	atexit(cwdaemon_close_libcw_output);

	cw_register_keying_callback(cwdaemon_keyingevent, NULL);

	cw_register_tone_queue_low_callback(cwdaemon_tone_queue_low_callback, NULL, 1);

	cwdaemon_debug(1, "Device used: %s", cwdev->desc);
	cwdev->init (cwdev, fd);

	if (forking)
	{
		pid = fork ();
		if (pid < 0)
		{
			printf ("%s: Fork failed: %s\n", PACKAGE,
				strerror (errno));
			exit (1);
		}

		if (pid > 0)
			exit (0);

		openlog ("netkeyer", LOG_PID, LOG_DAEMON);
		if ((sid = setsid ()) < 0)
		{
			syslog (LOG_ERR, "%s\n", "setsid");
			exit (1);
		}
		if ((chdir ("/")) < 0)
		{
			syslog (LOG_ERR, "%s\n", "chdir");
			exit (1);
		}
		umask (0);

		/* replace stdin/stdout/stderr with /dev/null */
		if ((fd = open("/dev/null", O_RDWR, 0)) == -1)
		{
			syslog (LOG_ERR, "%s\n", "open null");
			exit (1);
		}
		if (dup2 (fd, STDIN_FILENO) == -1)
		{
			syslog (LOG_ERR, "%s\n", "dup2 stdin");
			exit (1);
		}
		if (dup2 (fd, STDOUT_FILENO) == -1)
		{
			syslog (LOG_ERR, "%s\n", "dup2 stdout");
			exit (1);
		}
		if (dup2 (fd, STDERR_FILENO) == -1)
		{
			syslog (LOG_ERR, "%s\n", "dup2 stderr");
			exit (1);
		}
		if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
			(void)close (fd);
	}
	else
	{
		cwdaemon_debug(1, "Press ^C to quit");
		signal (SIGINT, catchint);
	}

	bzero (&request_addr, sizeof (request_addr));
	request_addr.sin_family = AF_INET;
	request_addr.sin_addr.s_addr = htonl (INADDR_ANY);
	request_addr.sin_port = htons (port);
	request_addrlen = sizeof (request_addr);

	socket_descriptor = socket (AF_INET, SOCK_DGRAM, 0);
	if (socket_descriptor == -1)
	{
		errmsg ("Socket open");
		exit (1);
	}

	if (bind(socket_descriptor, (struct sockaddr *) &request_addr,
		 sizeof (request_addr)) == -1)
	{
		errmsg ("Bind");
		exit (1);
	}

	save_flags = fcntl (socket_descriptor, F_GETFL);
	if (save_flags == -1)
	{
		errmsg ("Trying get flags");
		exit (1);
	}
	save_flags |= O_NONBLOCK;

	if (fcntl (socket_descriptor, F_SETFL, save_flags) == -1)
	{
		errmsg ("Trying non-blocking");
		exit (1);
	}

#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
	if (priority != 0)
	{
		if (setpriority (PRIO_PROCESS, getpid (), priority) < 0)
		{
			errmsg ("Setting priority");
			exit (1);
		}
	}
#endif

	request_queue[0] = '\0';
	do
	{
		fd_set readfd;
		struct timeval udptime;

		FD_ZERO (&readfd);
		FD_SET (socket_descriptor, &readfd);

		if (inactivity_seconds < 30)
		{
			udptime.tv_sec = 1;
			inactivity_seconds++;
		}
		else
			udptime.tv_sec = 86400;
		udptime.tv_usec = 0;
		/* udptime.tv_usec = 999000; */	/* 1s is more than enough */
		fd_count = select (socket_descriptor + 1, &readfd, NULL, NULL, &udptime);
		/* fd_count = select (socket_descriptor + 1, &readfd, NULL, NULL, NULL); */
		if (fd_count == -1 && errno != EINTR)
			errmsg ("Select");
		else
			cwdaemon_receive();

#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
		if (cwdev->footswitch)
			cwdev->ptt (cwdev, !((cwdev->footswitch(cwdev))));
#endif

	} while (1);

	cwdev->free (cwdev);
	if (close (socket_descriptor) == -1)
	{
		errmsg ("Close socket");
		exit (1);
	}
	exit (0);
}





/* *** unused code *** */





#if 0

/* some simple timing utilities, see
 * http://www.erlenstar.demon.co.uk/unix/faq_8.html */
static void
timer_add (struct timeval *tv, long secs, long usecs)
{
	tv->tv_sec += secs;
	if ((tv->tv_usec += usecs) >= 1000000)
	{
		tv->tv_sec += tv->tv_usec / 1000000;
		tv->tv_usec %= 1000000;
	}
}

/* Set *RES = *A - *B, returning the sign of the result */
static int
timer_sub (struct timeval *res, const struct timeval *a,
	   const struct timeval *b)
{
	long sec = a->tv_sec - b->tv_sec;
	long usec = a->tv_usec - b->tv_usec;

	if (usec < 0)
		usec += 1000000, --sec;

	res->tv_sec = sec;
	res->tv_usec = usec;

	return (sec < 0) ? (-1) : ((sec == 0 && usec == 0) ? 0 : 1);
}

#endif
