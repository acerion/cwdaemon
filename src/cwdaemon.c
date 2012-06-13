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
#define CWDAEMON_MIN_MORSE_SPEED     CW_SPEED_MIN /* [wpm], from libcw.h */
#define CWDAEMON_MAX_MORSE_SPEED     CW_SPEED_MAX /* [wpm], from libcw.h */
#define CWDAEMON_DEFAULT_MORSE_TONE           800 /* [Hz] */
#define CWDAEMON_DEFAULT_MORSE_VOLUME          70 /* [%] */
#define CWDAEMON_DEFAULT_PTT_DELAY              0 /* [ms] */
#define CWDAEMON_DEFAULT_MORSE_WEIGHTING        0 /* in range -50/+50 */

#define CWDAEMON_DEFAULT_NETWORK_PORT              6789
#define CWDAEMON_DEFAULT_AUDIO_SYSTEM  CW_AUDIO_CONSOLE /* Console buzzer, from libcw.h. */

#define CWDAEMON_USECS_PER_MSEC         1000 /* Just to avoid magic numbers. */
#define CWDAEMON_MESSAGE_SIZE_MAX        256 /* Maximal size of single message. */
#define CWDAEMON_REQUEST_QUEUE_SIZE_MAX 4000 /* Maximal size of common buffer/fifo where requests may be pushed to. */


/* Default values of parameters, may be modified only through
   commandline arguments passed to cwdaemon.
   These values are used when resetting libcw and cwdaemon to
   initial state. */
static int default_morse_speed  = CWDAEMON_DEFAULT_MORSE_SPEED;
static int default_morse_tone   = CWDAEMON_DEFAULT_MORSE_TONE;
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
static int socket_descriptor;
static int port = CWDAEMON_DEFAULT_NETWORK_PORT;  /* default UDP port we listen on */

static struct sockaddr_in request_addr;
static socklen_t          request_addrlen;

static struct sockaddr_in reply_addr;
static socklen_t          reply_addrlen;

static char reply_buffer[CWDAEMON_MESSAGE_SIZE_MAX];


/* Various variables. */
static int wordmode = 0;               /* Start in character mode. */
static int forking = 1;                /* We fork by default. */
static int debuglevel = 0;             /* Only debug when not forking. */
static int process_priority = 0;       /* Scheduling priority of cwdaemon process. */
static int async_abort = 0;            /* Unused variable. It is used in patches/cwdaemon-mt.patch though. */
static int inactivity_seconds = 9999;  /* Inactive since nnn seconds. */

/* Incoming requests are stored in this pseudo-FIFO before they are played. */
static char request_queue[CWDAEMON_REQUEST_QUEUE_SIZE_MAX];


/* flags for different states */
static unsigned char ptt_flag = 0;	/* flag for PTT state/behaviour */
/* PTT on while sending, delay performed, switch PTT off when
   libcw's queue of characters becomes empty. */
#define PTT_ACTIVE_AUTO		0x01
/* PTT manually activated, switch off manually. */
#define PTT_ACTIVE_MANUAL	0x02
/* We must return an echo when libcw's queue of characters becomes empty. */
#define PTT_ACTIVE_ECHO		0x04


void cwdaemon_set_ptt_on(cwdevice *device, char *info);
void cwdaemon_set_ptt_off(cwdevice *device, char *info);
void cwdaemon_switch_band(cwdevice *device, unsigned int band);

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
int  cwdaemon_set_default_cwdevice_descriptions(void);
void cwdaemon_free_cwdevice_descriptions(void);
int  cwdaemon_set_cwdevice(cwdevice **device, char *desc);
int  cwdaemon_initialize_socket(void);

int  cwdaemon_open_libcw_output(int audio_system);
void cwdaemon_close_libcw_output(void);
void cwdaemon_reset_libcw_output(void);

int  cwdaemon_get_long(const char *buf, long *lvp);
void cwdaemon_parse_command_line(int argc, char *argv[]);
void cwdaemon_print_help(void);

RETSIGTYPE cwdaemon_catchint(int signal);


cwdevice cwdevice_ttys = {
	.init       = ttys_init,
	.free       = ttys_free,
	.reset      = ttys_reset,
	.cw         = ttys_cw,
	.ptt        = ttys_ptt,
	.ssbway     = NULL,
	.switchband = NULL,
	.footswitch = NULL,
	.fd         = 0,
	.desc       = NULL
};

cwdevice cwdevice_null = {
	.init       = null_init,
	.free       = null_free,
	.reset      = null_reset,
	.cw         = null_cw,
	.ptt        = null_ptt,
	.ssbway     = NULL,
	.switchband = NULL,
	.footswitch = NULL,
	.fd         = 0,
	.desc       = NULL
};

#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
cwdevice cwdevice_lp = {
	.init       = lp_init,
	.free       = lp_free,
	.reset      = lp_reset,
	.cw         = lp_cw,
	.ptt        = lp_ptt,
	.ssbway     = lp_ssbway,
	.switchband = lp_switchband,
	.footswitch = lp_footswitch,
	.fd         = 0,
	.desc       = NULL
};
#endif



/* Selected keying device:
   serial port (cwdevice_ttys) || parallel port (cwdevice_lp) || null (cwdevice_null). */
static cwdevice *global_cwdevice = NULL;





/* catch ^C when running in foreground */
RETSIGTYPE cwdaemon_catchint(int signal)
{
	global_cwdevice->free(global_cwdevice);
	printf("%s: Exiting\n", PACKAGE);
	exit(EXIT_SUCCESS);
}





/* print error message to the console or syslog if we are forked */
void cwdaemon_errmsg(char *info, ...)
{
	va_list ap;
	char s[1025];

	va_start(ap, info);
	vsnprintf(s, 1024, info, ap);
	va_end(ap);

	if (forking) {
		syslog(LOG_ERR, "%s\n", s);
	} else {
		printf("%s: %s failed: %s\n", PACKAGE, s, strerror(errno));
	}

	return;
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
			cwdaemon_errmsg("Nanosleep");
		}
	}

	return;
}





/**
   \brief Band switch function using LPT port

   Band switch function using LPT (parallel) port of PC computer.

   In general, data is transmitted through LPT using pins 9 (MSB) - 2 (LSB).
   It seems that "TR Log" software has established a standard for controlling
   band switches using LPT port (or "TR Log" has just followed already
   established standard. Original information in cwdaemon's documentation
   mentions: "Pinout is compatible with the standard (CT, TRlog).").

   cwdaemon follows the standard. The standard utilizes only a subset of
   the data pins: pins 9, 8, 7, and 2.

   From TR Log manual, version 6.79, Appendix A, Table A.3
   (the manual is available at www.trlog.com):

   Hex value is transmitted through pins 9 (MSB), 8, 7, and 2 (LSB)

            Band     Value
             160         1
              80         2
              40         3
              30         4
              20         5
              17         6
              15         7
              12         8
              10         9
               6         A
               2         B
             222         C
             432         D
             902         E
            1GHz         F
   Other or None         0

*/
#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
void cwdaemon_switch_band(cwdevice *device, unsigned int band)
{
	unsigned int bit_pattern = (band & 0x01) | ((band & 0x0e) << 4);
	if (device->switchband) {
		device->switchband(device, bit_pattern);
		cwdaemon_debug(1, "Set band switch to %x", band);
	} else {
		cwdaemon_debug(1, "Band switch output not implemented");
	}

	return;
}
#endif





/**
   \brief Switch PTT on

   \param info - debug information displayed when performing the switching
*/
void cwdaemon_set_ptt_on(cwdevice *device, char *info)
{
	if (current_ptt_delay && !(ptt_flag & PTT_ACTIVE_AUTO)) {
		device->ptt(device, ON);
		cwdaemon_debug(1, info);
		int rv = cw_queue_tone((current_ptt_delay * CWDAEMON_USECS_PER_MSEC) * 20, 0);	/* try to 'enqueue' delay */
		if (rv == CW_FAILURE) {			        /* old libcw may reject freq=0 */
			cwdaemon_debug(1, "cw_queue_tone failed: rv=%d errno=%s, using udelay instead",
				       rv, strerror(errno));
			cwdaemon_udelay(current_ptt_delay * CWDAEMON_USECS_PER_MSEC);
		}
		ptt_flag |= PTT_ACTIVE_AUTO;
	}

	return;
}





/**
   \brief Switch PTT off

   \param info - debug information displayed when performing the switching
*/
void cwdaemon_set_ptt_off(cwdevice *device, char *info)
{
	device->ptt(device, OFF);
	ptt_flag = 0;
	cwdaemon_debug(1, info);

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
		cwdaemon_set_ptt_on(global_cwdevice, "PTT (TUNE) on");

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
			cwdaemon_debug(1, "Delaying switching to OSS, please wait few seconds.");
			sleep(4);
			rv = cw_generator_new(audio_system, NULL);
			if (rv == CW_SUCCESS) {
				break;
			}
		}
	}
	if (rv != CW_FAILURE) {
		rv = cw_generator_start();
		cwdaemon_debug(1, "Starting generator with sound system '%s': %s", cw_get_audio_system_label(audio_system), rv ? "success" : "failure");
	} else {
		/* FIXME:
		   When cwdaemon failed to create a generator, and user
		   kills non-forked cwdaemon through Ctrl+C, there is
		   a memory protection error. */
		cwdaemon_debug(1, "Failed to create generator with sound system '%s'", cw_get_audio_system_label(audio_system));
	}

	return rv == CW_FAILURE ? -1 : 0;
}





void cwdaemon_close_libcw_output(void)
{
	cw_generator_stop();
	cw_generator_delete();

	return;
}





/**
   \brief Reset parameters of libcw to default values

   Function uses values of cwdaemon's global 'default_' variables, and some
   other values to reset state of libcw.
*/
void cwdaemon_reset_libcw_output(void)
{
	/* This function is called when cwdaemon receives '0' escape code.
	   README describes this code as "Reset to default values".
	   Therefore we use default_* below. */

	/* Delete old generator (if it exists). */
	cwdaemon_close_libcw_output();

	cwdaemon_debug(3, "Setting sound system '%s'", cw_get_audio_system_label(default_audio_system));
	cwdaemon_open_libcw_output(default_audio_system);

	cw_set_frequency(default_morse_tone);
	cw_set_send_speed(default_morse_speed);
	cw_set_volume(default_morse_volume);
	cw_set_gap(0);
	cw_set_weighting(default_weighting * 0.6 + 50);

	return;
}





/* properly parse a 'long' integer */
int cwdaemon_get_long(const char *buf, long *lvp)
{
	errno = 0;

	char *ep;
	long lv = strtol(buf, &ep, 10);
	if (buf[0] == '\0' || *ep != '\0') {
		return -1;
	}

	if (errno == ERANGE && (lv == LONG_MAX || lv == LONG_MIN)) {
		return -1;
	}
	*lvp = lv;

	return 0;
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

   \param reply - buffer for reply (allocated by caller)
   \param request - buffer with request
   \param n - size of data in request
*/
void cwdaemon_prepare_reply(char *reply, const char *request, size_t n)
{
	memcpy(&reply_addr, &request_addr, sizeof(reply_addr)); /* Remember sender. */
	reply_addrlen = request_addrlen;

	strncpy(reply, request, n);
	cwdaemon_debug(2, "request='%s', reply='%s'", request, reply);
	cwdaemon_debug(1, "now waiting for end of transmission before echo");

	ptt_flag |= PTT_ACTIVE_ECHO; /* wait for tone queue to become empty, then echo back */

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
				   /* TODO: request_addrlen may be modified. Check it. */
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
			cwdaemon_errmsg("Recvfrom");
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
	} else {
		; /* pass */
	}

	request_buffer[recv_rc] = '\0';

	if (request_buffer[0] != 27) {
		/* No ESCAPE. All received data should be treated
		   as text to be sent using Morse code. */
		cwdaemon_debug(1, "Request = %s", request_buffer);
		if ((strlen(request_buffer) + strlen(request_queue)) <= CWDAEMON_REQUEST_QUEUE_SIZE_MAX - 1) {
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
		global_cwdevice->reset(global_cwdevice);
		ptt_flag = 0;
		cwdaemon_debug(1, "Reset all values");
		break;
	case '2':
		/* Set speed of morse code, in words per minute. */
		if (cwdaemon_get_long(request + 2, &lv)) {
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
		if (cwdaemon_get_long(request + 2, &lv)) {
			break;
		}

		if (lv > 0 && lv <= CW_FREQUENCY_MAX) {
			current_morse_tone = lv;
			cw_set_frequency(current_morse_tone);
			cwdaemon_debug(1, "Tone: %s Hz", request + 2);

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
				cwdaemon_set_ptt_off(global_cwdevice, "PTT off");
			}
			ptt_flag &= 0;
		}
		break;
	case '5':
		/* Exit cwdaemon. */
		global_cwdevice->free(global_cwdevice);
		errno = 0;
		cwdaemon_errmsg("Sender has told me to end the connection");
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
		if (cwdaemon_get_long(request + 2, &lv)) {
			break;
		}

		if ((lv > -51) && (lv < 51)) {
			cw_set_weighting(lv * 0.6 + 50);
			cwdaemon_debug(1, "Weight: %ld", lv);
		}
		break;
	case '8': {
		/* Set type of keying device. */
		cwdaemon_debug(1, "Setting new keying device: %s", request + 2);
		cwdaemon_set_cwdevice(&global_cwdevice, request + 2);

		break;
	}
	case '9':
		/* Base port number.
		   TODO: why this is obsolete? */
		cwdaemon_debug(1, "Obsolete control data '9'.");
		break;
	case 'a':	/* PTT keying on or off */
		if (cwdaemon_get_long(request + 2, &lv)) {
			break;
		}

		if (lv) {
			global_cwdevice->ptt(global_cwdevice, ON);
			if (current_ptt_delay) {
				cwdaemon_set_ptt_on(global_cwdevice, "PTT (manual, delay) on");
			} else {
				cwdaemon_debug(1, "PTT (manual, immediate) on");
			}
			ptt_flag |= PTT_ACTIVE_MANUAL;
		} else if (ptt_flag & PTT_ACTIVE_MANUAL) {	/* only if manually activated */
			ptt_flag &= ~PTT_ACTIVE_MANUAL;
			if (!(ptt_flag & !PTT_ACTIVE_AUTO)) {	/* no PTT modifiers */

				if (request_queue[0] == '\0'/* no new text in the meantime */
				    && cw_get_tone_queue_length() <= 1) {

					cwdaemon_set_ptt_off(global_cwdevice, "PTT (manual, immediate) off");
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
		if (cwdaemon_get_long(request + 2, &lv)) {
			break;
		}

		if (lv) {
			if (global_cwdevice->ssbway) {
				global_cwdevice->ssbway(global_cwdevice, SOUNDCARD);
				cwdaemon_debug(1, "SSB way set to SOUNDCARD", PACKAGE);
			} else {
				cwdaemon_debug(1, "SSB way to SOUNDCARD unimplemented");
			}
		} else {
			if (global_cwdevice->ssbway) {
				global_cwdevice->ssbway(global_cwdevice, MICROPHONE);
				cwdaemon_debug(1, "SSB way set to MIC");
			} else {
				cwdaemon_debug(1, "SSB way to MICROPHONE unimplemented");
			}
		}
#else
		cwdaemon_debug(1, "Escape code 'b' unavailable (no parallel port available).");
#endif
		break;
	case 'c':
		/* Tune for a number of seconds */
		if (cwdaemon_get_long(request + 2, &lv)) {
			break;
		}

		if (lv <= 10) {
			cwdaemon_tune(lv);
		}
		break;
	case 'd':
		/* Set PTT delay (TOD, Turn On Delay).
		   The value is milliseconds. */
		if (cwdaemon_get_long(request + 2, &lv)) {
			break;
		}

		if (lv >= 0 && lv < 51) {
			current_ptt_delay = lv;
		} else {
			current_ptt_delay = 50;
		}

		cwdaemon_debug(1, "PTT delay(TOD): %d ms", current_ptt_delay);

		if (current_ptt_delay == 0) {
			cwdaemon_set_ptt_off(global_cwdevice, "ensure PTT off");
		}
		break;
	case 'e':	/* set bandswitch output on parport bits 2(lsb),7,8,9(msb) */
#if defined(HAVE_LINUX_PPDEV_H) || defined(HAVE_DEV_PPBUS_PPI_H)
		if (cwdaemon_get_long(request + 2, &lv)) {
			break;
		}

		if (lv <= 15 && lv >= 0) {
			cwdaemon_switch_band(global_cwdevice, lv);
		}
#else
		cwdaemon_debug(1, "Band switching through parallel port is unavailable.");
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
		cwdaemon_debug(1, "Switching to sound system '%s'", cw_get_audio_system_label(current_audio_system));
		cwdaemon_close_libcw_output();
		cwdaemon_open_libcw_output(current_audio_system);
		break;
	}
	case 'g':
		/* Set volume of sound, in percents. */
		if (cwdaemon_get_long(request + 2, &lv)) {
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
			/* Speed increase & decrease */
			/* Repeated '+' and '-' characters are allowed,
			   in such cases increase and decrease of speed is
			   multiple of 2 wpm. */
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
			cwdaemon_set_ptt_on(global_cwdevice, "PTT (auto) on");
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
		global_cwdevice->cw(global_cwdevice, ON);
	} else {
		global_cwdevice->cw(global_cwdevice, OFF);
	}

	inactivity_seconds = 0;

	cwdaemon_debug(3, "keying event %d", keystate);

	return;
}





/* Callback routine when tone queue is empty */
void cwdaemon_tone_queue_low_callback(void *arg)
{
	cwdaemon_debug(2, "Entering \"queue empty\" callback, ptt_flag=%02x", ptt_flag);
	if (ptt_flag == PTT_ACTIVE_AUTO        /* PTT on, w/o manual PTT or similar */
	    && request_queue[0] == '\0'        /* No new text in the meantime. */
	    && cw_get_tone_queue_length() <= 1) {

		cwdaemon_set_ptt_off(global_cwdevice, "PTT (auto) off");

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
	int p;

	while ((p = getopt(argc, argv, "d:hnp:P:s:t:T:v:Vw:x:")) != -1) {
		long lv;

		switch (p) {
		case ':':
		case '?':
		case 'h':
			cwdaemon_print_help();
			exit(EXIT_SUCCESS);
		case 'd':
			if (cwdaemon_set_cwdevice(&global_cwdevice, optarg) == -1) {
				exit(EXIT_FAILURE);
			}
			break;
		case 'n':
			if (forking) {
				printf("%s: Not forking...\n", PACKAGE);
				forking = 0;
			}
			debuglevel++;
			break;
		case 'p':
			if (cwdaemon_get_long(optarg, &lv) || lv < 1024 || lv > 65536) {
				printf("%s: invalid port number - %s\n",
				       PACKAGE, optarg);
				exit(EXIT_FAILURE);
			}
			port = lv;
			break;
#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
		case 'P':
			if (cwdaemon_get_long(optarg, &lv) || lv < -20 || lv > 20) {
				printf("%s: bad priority %s (should be between -20 and 20 inclusive)\n",
				       PACKAGE, optarg);
				exit(EXIT_FAILURE);
			}
			process_priority = lv;
			break;
#endif
		case 's':
			if (cwdaemon_get_long(optarg, &lv) || lv < CWDAEMON_MIN_MORSE_SPEED || lv > CWDAEMON_MAX_MORSE_SPEED) {
				printf("%s: bad speed %s (should be between %d and %d inclusive)\n",
				       PACKAGE, optarg,
				       CWDAEMON_MIN_MORSE_SPEED, CWDAEMON_MAX_MORSE_SPEED);
				exit(EXIT_FAILURE);
			}
			default_morse_speed = lv;
			break;
		case 't':
			if (cwdaemon_get_long(optarg, &lv) || lv < 0 || lv > 50) {
				printf("%s: bad PTT delay value %s (should be between 0 and 50 ms inclusive)\n",
				       PACKAGE, optarg);
				exit(EXIT_FAILURE);
			}
			default_ptt_delay = lv;
			break;
		case 'v':
			if (cwdaemon_get_long(optarg, &lv) || lv < 0 || lv > 100) {
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
			if (cwdaemon_get_long(optarg, &lv) || lv < -50 || lv > 50) {
				printf("%s: bad weight %s (should be between -50 and 50 inclusive)\n",
				       PACKAGE, optarg);
				exit(EXIT_FAILURE);
			}
			default_weighting = lv;
			break;
		case 'T':
			if (cwdaemon_get_long(optarg, &lv) || lv < CW_FREQUENCY_MIN || lv > CW_FREQUENCY_MAX) {
				printf("%s: bad tone %s (should be between %d and %d inclusive)\n",
				       PACKAGE, optarg, CW_FREQUENCY_MIN, CW_FREQUENCY_MAX);
				exit(EXIT_FAILURE);
			}
			default_morse_tone = lv;
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
	printf("-x <sound system>\n");
	printf("        Use a specific sound system:\n");
	printf("        c = console buzzer (default)\n");
	printf("        o = OSS\n");
	printf("        a = ALSA\n");
	printf("        p = PulseAudio\n");
	printf("        n = none (no audio)\n");
	printf("        s = soundcard (autoselect from OSS/ALSA/PulseAudio)\n");
	printf("-v <volume>\n");
	printf("        Set volume for soundcard output\n");
	printf("-w <weight>\n");
	printf("        Set weighting (-50 ... 50, default = 0)\n");
	printf("-T <tone>\n");
	printf("        Set initial tone to 'tone' (%d - %d Hz, default: %d).\n",
	       CW_FREQUENCY_MIN, CW_FREQUENCY_MAX, CWDAEMON_DEFAULT_MORSE_TONE);


	return;
}





/* main program: fork, open network connection and go into an endless loop
   waiting for something to happen on the UDP port */
int main(int argc, char *argv[])
{
	atexit(cwdaemon_free_cwdevice_descriptions);
	if (cwdaemon_set_default_cwdevice_descriptions() == -1) {
		exit(EXIT_FAILURE);
	}

	cwdaemon_parse_command_line(argc, argv);

	if (debuglevel > 3) {		/* debugging cwlib as well */
		/* FIXME: pass correct libcw debug flags. */
		cw_set_debug_flags((1 << (debuglevel - 3)) -1);
	}


	cwdaemon_reset_almost_all();
	atexit(cwdaemon_close_libcw_output);

	cw_register_keying_callback(cwdaemon_keyingevent, NULL);
	cw_register_tone_queue_low_callback(cwdaemon_tone_queue_low_callback, NULL, 1);

	if (forking) {
		pid_t pid = fork();
		if (pid < 0) {
			printf("%s: Fork failed: %s\n", PACKAGE,
			       strerror(errno));
			exit(EXIT_FAILURE);
		}

		if (pid > 0) {
			/* We are the parent process. The process is
			   no longer needed at this point. */
			exit(EXIT_SUCCESS);
		}

		openlog("netkeyer", LOG_PID, LOG_DAEMON);
		pid_t sid = setsid();
		if (sid < 0) {
			syslog(LOG_ERR, "%s\n", "setsid");
			exit(EXIT_FAILURE);
		}

		if ((chdir("/")) < 0) {
			syslog(LOG_ERR, "%s\n", "chdir");
			exit(EXIT_FAILURE);
		}
		umask(0);

		int fd;
		/* Replace stdin/stdout/stderr with /dev/null. */
		if ((fd = open("/dev/null", O_RDWR, 0)) == -1) {
			syslog(LOG_ERR, "%s\n", "open /dev/null");
			exit(EXIT_FAILURE);
		}
		if (dup2(fd, STDIN_FILENO) == -1) {
			syslog(LOG_ERR, "%s\n", "dup2 stdin");
			exit(EXIT_FAILURE);
		}
		if (dup2(fd, STDOUT_FILENO) == -1) {
			syslog(LOG_ERR, "%s\n", "dup2 stdout");
			exit(EXIT_FAILURE);
		}
		if (dup2(fd, STDERR_FILENO) == -1) {
			syslog(LOG_ERR, "%s\n", "dup2 stderr");
			exit(EXIT_FAILURE);
		}
		if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
			(void) close(fd);
		}
	} else {
		cwdaemon_debug(1, "Press ^C to quit");
		signal(SIGINT, cwdaemon_catchint);
	}

	if (cwdaemon_initialize_socket() == -1) {
		exit(CW_FAILURE);
	}

#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
	if (process_priority != 0) {
		if (setpriority(PRIO_PROCESS, getpid(), process_priority) < 0) {
			cwdaemon_errmsg("Setting process priority: \"%s\"", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
#endif

	request_queue[0] = '\0';
	do {
		fd_set readfd;
		struct timeval udptime;

		FD_ZERO(&readfd);
		FD_SET(socket_descriptor, &readfd);

		if (inactivity_seconds < 30) {
			udptime.tv_sec = 1;
			inactivity_seconds++;
		} else {
			udptime.tv_sec = 86400;
		}

		udptime.tv_usec = 0;
		/* udptime.tv_usec = 999000; */	/* 1s is more than enough */
		int fd_count = select(socket_descriptor + 1, &readfd, NULL, NULL, &udptime);
		/* fd_count = select (socket_descriptor + 1, &readfd, NULL, NULL, NULL); */
		if (fd_count == -1 && errno != EINTR) {
			cwdaemon_errmsg("Select");
		} else {
			cwdaemon_receive();
		}

#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
		if (global_cwdevice->footswitch) {
			int state = global_cwdevice->footswitch(global_cwdevice);
			global_cwdevice->ptt(global_cwdevice, !state);
		}
#endif

	} while (1);

	global_cwdevice->free(global_cwdevice);
	if (close(socket_descriptor) == -1) {
		cwdaemon_errmsg("Close socket");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}





int cwdaemon_set_default_cwdevice_descriptions(void)
{
	/* Default device description of parallel/lpt port. */
#if defined HAVE_LINUX_PPDEV_H        /* Linux, obviously. */
	cwdevice_lp.desc = strdup("parport0");
#elif defined HAVE_DEV_PPBUS_PPI_H    /* FreeBSD (ppbus/ppi). */
	cwdevice_lp.desc = strdup("ppi0");
#else                                 /* FIXME: where is OpenBSD? */
	cwdevice_lp.desc = NULL;
#endif


	/* Default device description of serial/com port. */
#if defined(__linux__)
	cwdevice_ttys.desc = strdup("ttyS0");
#elif defined(__FreeBSD__)
        cwdevice_ttys.desc = strdup("ttyd0");
#elif defined(__OpenBSD__)
	cwdevice_ttys.desc = strdup("tty00");
#else
	cwdevice_ttys.desc = NULL;
#endif


	/* Default device description of null port. */
	cwdevice_null.desc = strdup("null");


	/* TODO: add checks of return values. */

	return 0;
}





void cwdaemon_free_cwdevice_descriptions(void)
{
	if (cwdevice_ttys.desc) {
		free(cwdevice_ttys.desc);
		cwdevice_ttys.desc = NULL;
	}

	if (cwdevice_null.desc) {
		free(cwdevice_null.desc);
		cwdevice_null.desc = NULL;
	}

	if (cwdevice_lp.desc) {
		free(cwdevice_lp.desc);
		cwdevice_lp.desc = NULL;
	}

	return;
}





int cwdaemon_set_cwdevice(cwdevice **device, char *desc)
{
	int fd;

	if ((fd = dev_is_tty(desc)) != -1) {
		*device = &cwdevice_ttys;
	}
#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
	else if ((fd = dev_is_parport(desc)) != -1) {
		if (geteuid()) {
			printf("You must run this program as root to use parallel port.\n");
			return -1;
		}
		*device = &cwdevice_lp;
	}
#endif
	else if ((fd = dev_is_null(desc)) != -1) {
		*device = &cwdevice_null;
	} else {
		; /* Pass, checking for NULL device below. */
	}

	if (!*device) {
		printf("%s: bad keyer device: %s\n", PACKAGE, desc);
		return -1;
	}

	if ((*device)->desc) {
		free((*device)->desc);
	}
	(*device)->desc = strdup(desc);

	cwdaemon_debug(1, "Keying device used: %s", (*device)->desc);
	(*device)->init(*device, fd);

	return 0;

}





int cwdaemon_initialize_socket(void)
{
	bzero(&request_addr, sizeof (request_addr));
	request_addr.sin_family = AF_INET;
	request_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	request_addr.sin_port = htons(port);
	request_addrlen = sizeof (request_addr);

	socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_descriptor == -1) {
		cwdaemon_errmsg("Socket open");
		return -1;
	}

	if (bind(socket_descriptor,
		 (struct sockaddr *) &request_addr,
		 request_addrlen) == -1) {

		cwdaemon_errmsg("Bind");
		return -1;
	}

	int save_flags = fcntl(socket_descriptor, F_GETFL);
	if (save_flags == -1) {
		cwdaemon_errmsg("Trying get flags");
		return -1;
	}
	save_flags |= O_NONBLOCK;

	if (fcntl(socket_descriptor, F_SETFL, save_flags) == -1) {
		cwdaemon_errmsg("Trying non-blocking");
		return -1;
	}

	return 0;
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
