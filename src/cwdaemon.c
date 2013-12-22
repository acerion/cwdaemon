/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2013 Kamil Ignacak <acerion@wp.pl>
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

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#define _POSIX_C_SOURCE 200809L /* nanosleep(), strdup() */
#define _GNU_SOURCE /* getopt_long() */

#include "config.h"

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

#if defined(HAVE_GETOPT_H)
#include <getopt.h>
#endif

#include <libcw.h>
#include <libcw_debug.h>
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

   Size of a message is not constant.
   Maximal size of a message is CWDAEMON_MESSAGE_SIZE_MAX.
*/




/* cwdaemon constants. */
#define CWDAEMON_MORSE_SPEED_DEFAULT           24 /* [wpm] */
#define CWDAEMON_MORSE_TONE_DEFAULT           800 /* [Hz] */
#define CWDAEMON_MORSE_VOLUME_DEFAULT          70 /* [%] */

/* TODO: why the limitation to 50 ms? Is it enough? */
#define CWDAEMON_PTT_DELAY_DEFAULT              0 /* [ms] */
#define CWDAEMON_PTT_DELAY_MIN                  0 /* [ms] */
#define CWDAEMON_PTT_DELAY_MAX                 50 /* [ms] */

/* Notice that the range accepted by cwdaemon is different than that
   accepted by libcw. */
#define CWDAEMON_MORSE_WEIGHTING_DEFAULT        0
#define CWDAEMON_MORSE_WEIGHTING_MIN          -50
#define CWDAEMON_MORSE_WEIGHTING_MAX           50

#define CWDAEMON_NETWORK_PORT_DEFAULT              6789
#define CWDAEMON_AUDIO_SYSTEM_DEFAULT  CW_AUDIO_CONSOLE /* Console buzzer, from libcw.h. */

#define CWDAEMON_USECS_PER_MSEC         1000 /* Just to avoid magic numbers. */
#define CWDAEMON_USECS_PER_SEC       1000000 /* Just to avoid magic numbers. */
#define CWDAEMON_MESSAGE_SIZE_MAX        256 /* Maximal size of single message. */
#define CWDAEMON_REQUEST_QUEUE_SIZE_MAX 4000 /* Maximal size of common buffer/fifo where requests may be pushed to. */


/* For developer's debugging purposes. */
//extern cw_debug_t cw_debug_object_dev;


/* Default values of parameters, may be modified only through
   commandline arguments passed to cwdaemon.
   These values are used when resetting libcw and cwdaemon to
   initial state. */
static int default_morse_speed  = CWDAEMON_MORSE_SPEED_DEFAULT;
static int default_morse_tone   = CWDAEMON_MORSE_TONE_DEFAULT;
static int default_morse_volume = CWDAEMON_MORSE_VOLUME_DEFAULT;
static int default_ptt_delay    = CWDAEMON_PTT_DELAY_DEFAULT;
static int default_audio_system = CWDAEMON_AUDIO_SYSTEM_DEFAULT;
static int default_weighting    = CWDAEMON_MORSE_WEIGHTING_DEFAULT;


/* Actual values of parameters, used to control ongoing operation of
   cwdaemon+libcw. These values can be modified through requests
   received from socket in cwdaemon_receive(). */
static int current_morse_speed  = CWDAEMON_MORSE_SPEED_DEFAULT;
static int current_morse_tone   = CWDAEMON_MORSE_TONE_DEFAULT;
static int current_morse_volume = CWDAEMON_MORSE_VOLUME_DEFAULT;
static int current_ptt_delay    = CWDAEMON_PTT_DELAY_DEFAULT;
static int current_audio_system = CWDAEMON_AUDIO_SYSTEM_DEFAULT;


/* Level of libcw's tone queue that triggers 'callback for low level
   in tone queue'.  The callback function is
   cwdaemon_tone_queue_low_callback(), it is registered with
   cw_register_tone_queue_low_callback().

   I REALLY don't think that you would want to set it to any value
   other than '1'. */
static const int tq_low_watermark = 1;

/* Quick and dirty solution to following problem: when cwdaemon for
   some reason fails to open audio output, and attempts to play
   characters received from client, it crashes.  It doesn't know that
   it attempts to play to closed audio output.

   This is a flag telling cwdaemon if audio output is available or not. */
static bool has_audio_output = false;


/* Network variables. */
/* cwdaemon usually receives requests from client, but on occasions
   it needs to send a reply back. This is why in addition to
   request_* we also have reply_* */
static int socket_descriptor;
static int port = CWDAEMON_NETWORK_PORT_DEFAULT;  /* default UDP port we listen on */

static struct sockaddr_in request_addr;
static socklen_t          request_addrlen;

static struct sockaddr_in reply_addr;
static socklen_t          reply_addrlen;

static char reply_buffer[CWDAEMON_MESSAGE_SIZE_MAX];



/* cwdaemon may print debug messages to a disc file instead of stdout. */
static FILE *cwdaemon_debug_f = NULL;
static char *cwdaemon_debug_f_path = NULL;


static const char *cwdaemon_verbosity_labels[] = {
	"NN", /* None - obviously this label will never be used. */
	"EE",
	"WW",
	"II",
	"DD" };


static long int libcw_debug_flags = 0;



/* Various variables. */
static int wordmode = 0;               /* Start in character mode. */
static int forking = 1;                /* We fork by default. */
static int cwdaemon_verbosity = CWDAEMON_VERBOSITY_I;   /* Threshold of verbosity of debug messages. */
static int process_priority = 0;       /* Scheduling priority of cwdaemon process. */
static int async_abort = 0;            /* Unused variable. It is used in patches/cwdaemon-mt.patch though. */
static int inactivity_seconds = 9999;  /* Inactive since nnn seconds. */


/* Incoming requests without Escape code are stored in this pseudo-FIFO
   before they are played. */
static char request_queue[CWDAEMON_REQUEST_QUEUE_SIZE_MAX];



/* Flag for PTT state/behaviour. */
static unsigned char ptt_flag = 0;

/* Automatically turn PTT on and off.
   Turn PTT on when starting to play Morse characters, and turn PTT off when
   there are no more characters to play.
   "Automatically" means that cwdaemon toggles PTT without any additional
   actions taken by client. Client doesn't have to tell cwdaemon when to
   turn PTT on/off - this is done by cwdaemon itself, automatically.

   If ptt delay is non-zero, cwdaemon performs delay between turning PTT on
   and starting to play Morse characters.
   TODO: is there a delay before turning PTT off? */
#define PTT_ACTIVE_AUTO		0x01

/* PTT is turned on and off manually.
   It is the client who decides when to turn the PTT on and off.
   The client has to send 'a' escape code, followed by '1' or '0' to
   'manually' turn PTT on or off.
   'MANUAL' - the opposite of 'AUTO' where it is cwdaemon that decides
   when to turn PTT on and off.
   Perhaps "PTT_ON_REQUEST" would be a better name of the constant. */
#define PTT_ACTIVE_MANUAL	0x02

/* Don't turn PTT off until cwdaemon sends back an echo to client.
   client may request echoing back to it a reply when cwdaemon finishes
   playing given request. PTT shouldn't be turned off when sending the
   reply (TODO: why it shouldn't?). */
#define PTT_ACTIVE_ECHO		0x04



void cwdaemon_set_ptt_on(cwdevice *device, const char *info);
void cwdaemon_set_ptt_off(cwdevice *device, const char *info);
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
int  cwdaemon_set_cwdevice(cwdevice **device, const char *desc);
int  cwdaemon_initialize_socket(void);

int  cwdaemon_open_libcw_output(int audio_system);
void cwdaemon_close_libcw_output(void);
void cwdaemon_reset_libcw_output(void);

int  cwdaemon_get_long(const char *buf, long *lvp);

static void cwdaemon_args_parse(int argc, char *argv[]);
static void cwdaemon_args_help(void);
static bool cwdaemon_args_cwdevice(const char *optarg);
static void cwdaemon_args_nofork(void);
static bool cwdaemon_args_port(const char *optarg);
static bool cwdaemon_args_priority(int *priority, const char *optarg);
static bool cwdaemon_args_wpm(int *wpm, const char *optarg);
static bool cwdaemon_args_pttdelay(int *delay, const char *optarg);
static bool cwdaemon_args_volume(int *volume, const char *optarg);
static void cwdaemon_args_version(void);
static bool cwdaemon_args_weighting(int *weighting, const char *optarg);
static bool cwdaemon_args_tone(int *tone, const char *optarg);
static void cwdaemon_args_inc_verbosity(void);
static bool cwdaemon_args_set_verbosity(const char *optarg);
static bool cwdaemon_args_libcwflags(const char *optarg);
static bool cwdaemon_args_debugfile(const char *optarg);
static bool cwdaemon_args_system(int *system, const char *optarg);

static void cwdaemon_args_process_short(int c, const char *optarg);
static void cwdaemon_args_process_long(int argc, char *argv[]);

static void cwdaemon_debug_open(void);
static void cwdaemon_debug_close(void);

RETSIGTYPE cwdaemon_catch_sigint(int signal);


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
   serial port (cwdevice_ttys) || parallel port (cwdevice_lp) || null (cwdevice_null).
   It should be configured with cwdaemon_set_cwdevice(). */
/* FIXME: if no device is specified in command line, and no physical
   device is available, the global_cwdevice is NULL, which causes the
   program to break. */
static cwdevice *global_cwdevice = NULL;





/* catch ^C when running in foreground */
RETSIGTYPE cwdaemon_catch_sigint(__attribute__((unused)) int signal)
{
	global_cwdevice->free(global_cwdevice);
	printf("%s: Exiting\n", PACKAGE);
	exit(EXIT_SUCCESS);
}





/**
   \brief Print error message to the console or syslog

   Function checks if cwdaemon has forked, and prints given error message
   to stdout (if cwdaemon hasn't forked) or to syslog (if cwdaemon has
   forked).

   Function accepts printf-line formatting string as first argument, and
   a set of optional arguments to be inserted into the formatting string.

   \param info - first part of an error message, a formatting string
*/
void cwdaemon_errmsg(const char *info, ...)
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
		fflush(stdout);
	}

	return;
}





/**
   \brief Print debug message to debug file

   Function decides if given \p verbosity level is sufficient to print
   given \p format debug message, and prints it to stdout. If current
   global verbosity level is "None", no information will be printed.

   Currently \p verbosity can have one of values defined in enum
   cwdaemon_verbosity.

   Function accepts printf-line formatting string as last named
   argument (\p format), and a set of optional arguments to be
   inserted into the formatting string.

   \param verbosity - verbosity level of given message
   \param format - formatting string of a message being printed
*/
void cwdaemon_debug(int verbosity, const char *func, int line, const char *format, ...)
{
	if (cwdaemon_verbosity > CWDAEMON_VERBOSITY_N
	    && verbosity <= cwdaemon_verbosity
	    && cwdaemon_debug_f) {

		fprintf(cwdaemon_debug_f, "%s:%s: ", PACKAGE, cwdaemon_verbosity_labels[verbosity]);

		va_list ap;
		char s[1024 + 1];
		va_start(ap, format);
		vsnprintf(s, 1024, format, ap);
		va_end(ap);

		fprintf(cwdaemon_debug_f, s);
		fprintf(cwdaemon_debug_f, "\n");
		fprintf(cwdaemon_debug_f, "cwdaemon:        %s(): %d\n", func, line);
		fflush(cwdaemon_debug_f);
	}

	return;
}




void cwdaemon_debug_close(void)
{
	if (cwdaemon_debug_f
	    && cwdaemon_debug_f != stdout) {

		fclose(cwdaemon_debug_f);
		cwdaemon_debug_f = NULL;
	}

	return;
}




/**
   \brief Sleep for specified amount of microseconds

   Function can detect an interrupt from a signal, and continue sleeping,
   but only once.

   \param us - microseconds to sleep
*/
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



   The function works only for a subset of devices that are able to
   perform band switching. Currently the only such device is parallel port.

   \brief device - device to use to switch band
   \brief band - band number to switch to (a hex number)
*/
#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
void cwdaemon_switch_band(cwdevice *device, unsigned int band)
{
	unsigned int bit_pattern = (band & 0x01) | ((band & 0x0e) << 4);
	if (device->switchband) {
		device->switchband(device, bit_pattern);
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Set band switch to %x", band);
	} else {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__, "Band switch output not implemented");
	}

	return;
}
#endif





/**
   \brief Switch PTT on

   \param device - current keying device
   \param info - debug information displayed when performing the switching
*/
void cwdaemon_set_ptt_on(cwdevice *device, const char *info)
{
	/* For backward compatibility it is assumed that ptt_delay=0
	   means "cwdaemon shouldn't turn PTT on, at all". */

	if (current_ptt_delay && !(ptt_flag & PTT_ACTIVE_AUTO)) {
		device->ptt(device, ON);
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, info);

		int rv = cw_queue_tone(current_ptt_delay * CWDAEMON_USECS_PER_MSEC, 0);	/* try to 'enqueue' delay */
		if (rv == CW_FAILURE) {	/* Old libcw may reject freq=0. */
			cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
				       "cw_queue_tone failed: rv=%d errno=%s, using udelay instead",
				       rv, strerror(errno));
			/* TODO: wouldn't it be simpler to not to call
			   cw_queue_tone() and use only cwdaemon_udelay()? */
			cwdaemon_udelay(current_ptt_delay * CWDAEMON_USECS_PER_MSEC);
		}
		ptt_flag |= PTT_ACTIVE_AUTO;
		cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag +PTT_ACTIVE_AUTO (%02d, %d)", ptt_flag, __LINE__);
	}

	return;
}





/**
   \brief Switch PTT off

   \param device - current keying device
   \param info - debug information displayed when performing the switching
*/
void cwdaemon_set_ptt_off(cwdevice *device, const char *info)
{
	device->ptt(device, OFF);
	ptt_flag = 0;
	cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag =0 (%02d, %d)", ptt_flag, __LINE__);

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, info);

	return;
}





/**
   \brief Tune for a number of seconds

   Play a continuous sound for a given number of seconds.

   \param seconds - time of tuning
*/
void cwdaemon_tune(int seconds)
{
	if (seconds > 0) {
		cw_flush_tone_queue();
		cwdaemon_set_ptt_on(global_cwdevice, "PTT (TUNE) on");

		/* make it similar to normal CW, allowing interrupt */
		for (int i = 0; i < seconds; i++) {
			cw_queue_tone(CWDAEMON_USECS_PER_SEC, current_morse_tone);
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
		for (int i = 0; i < 5; i++) {
			cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Delaying switching to OSS, please wait few seconds.");
			sleep(4);
			rv = cw_generator_new(audio_system, NULL);
			if (rv == CW_SUCCESS) {
				break;
			}
		}
	}
	if (rv != CW_FAILURE) {
		rv = cw_generator_start();
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Starting generator with sound system '%s': %s", cw_get_audio_system_label(audio_system), rv ? "success" : "failure");

	} else {
		/* FIXME:
		   When cwdaemon failed to create a generator, and user
		   kills non-forked cwdaemon through Ctrl+C, there is
		   a memory protection error.

		   It seems that this error has been fixed with
		   changes in libcw, committed on 31.12.2012.
		   To be observed. */
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__, "Failed to create generator with sound system '%s'", cw_get_audio_system_label(audio_system));
	}

	return rv == CW_FAILURE ? -1 : 0;
}





/**
   \brief Close libcw audio output
*/
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

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Setting sound system '%s'", cw_get_audio_system_label(default_audio_system));

	if (!cwdaemon_open_libcw_output(default_audio_system)) {
		has_audio_output = true;
	} else {
		has_audio_output = false;
		return;
	}

	cw_set_frequency(default_morse_tone);
	cw_set_send_speed(default_morse_speed);
	cw_set_volume(default_morse_volume);
	cw_set_gap(0);
	cw_set_weighting(default_weighting * 0.6 + CWDAEMON_MORSE_WEIGHTING_MAX);

	return;
}





/**
   \brief Properly parse a 'long' integer

   Parse a string with digits, convert it to a long integer

   \param buf - input buffer with a string
   \param lvp - pointer to output long int variable

   \return -1 on failure
   \return 0 on success
*/
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
       should be played, but it also should be used as a reply.
       This function does not specify when the reply should be sent back.
       All it does is it prepares the text of reply.

       '^' can be used for char-by-char communication: client software
       message with single character followed by '^'. cwdaemon plays the
       character, and informs client software about playing the sound. Then
       client software can send request with next character followed by '^'.
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
	/* Since we need to prepare a reply, we need to mark our
	   intent to send echo. The echo (reply) will be sent to client
	   when libcw's tone queue becomes empty.

	   It is important to set this flag at the beginning of the function. */
	ptt_flag |= PTT_ACTIVE_ECHO;
	cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag +PTT_ACTIVE_ECHO (%02d, %d)", ptt_flag, __LINE__);

	memcpy(&reply_addr, &request_addr, sizeof(reply_addr)); /* Remember sender. */
	reply_addrlen = request_addrlen;

	strncpy(reply, request, n);
	reply[n] = '\0'; /* FIXME: where is boundary checking? */

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "text of request='%s', text of reply='%s'", request, reply);
	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "now waiting for end of transmission before echoing back to client");

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
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__, "sendto: %s", strerror(errno));
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





/**
   \brief Receive message from socket, act upon it

   Watch the socket and if there is an escape character check what it is,
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
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "...recv_from (no data)");
		return 0;
	} else {
		; /* pass */
	}

	request_buffer[recv_rc] = '\0';

	if (request_buffer[0] != 27) {
		/* No ESCAPE. All received data should be treated
		   as text to be sent using Morse code. */
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Request = '%s'", request_buffer);
		if ((strlen(request_buffer) + strlen(request_queue)) <= CWDAEMON_REQUEST_QUEUE_SIZE_MAX - 1) {
			strcat(request_queue, request_buffer);
			cwdaemon_play_request(request_queue);
		} else {
			; /* TODO: how to handle this case? */
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
	case '0':
		/* Reset all values. */
		request_queue[0] = '\0';
		cwdaemon_reset_almost_all();
		wordmode = 0;
		async_abort = 0;
		global_cwdevice->reset(global_cwdevice);

		ptt_flag = 0;
		cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag =0 (%02d, %d)", ptt_flag, __LINE__);

		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Reset all values");
		break;
	case '2':
		/* Set speed of Morse code, in words per minute. */
		if (cwdaemon_args_wpm(&current_morse_speed, request + 2)) {
			cw_set_send_speed(current_morse_speed);
			cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Speed: %d wpm", current_morse_speed);
		}
		break;
	case '3':
		/* Set tone (frequency) of morse code, in Hz.
		   The code assumes that minimal valid frequency is zero. */
		assert (CW_FREQUENCY_MIN == 0);
		if (cwdaemon_args_tone(&current_morse_tone, request + 2)) {
			if (current_morse_tone > 0) {

				cw_set_frequency(current_morse_tone);
				cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Tone: %l Hz", current_morse_tone);

				/* Should we really be adjusting volume when
				   the command is for frequency? It would be more
				   "elegant" not to do so. */
				cw_set_volume(current_morse_volume);

			} else { /* current_morse_tone==0, sidetone off */
				cw_set_volume(0);
				cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Volume off");
			}
		}
		break;
	case '4':
		/* Abort currently sent message. */
		if (wordmode) {
			cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Word mode - ignoring 'Message abort' request");
		} else {
			cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Character mode - message abort");
			if (ptt_flag & PTT_ACTIVE_ECHO) {
				cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Echo 'break'");
				cwdaemon_sendto("break\r\n");
			}
			request_queue[0] = '\0';
			cw_flush_tone_queue();
			cw_wait_for_tone_queue();
			if (ptt_flag) {
				cwdaemon_set_ptt_off(global_cwdevice, "PTT off");
			}
			ptt_flag &= 0;
			cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag =0 (%02d, %d)", ptt_flag, __LINE__);
		}
		break;
	case '5':
		/* Exit cwdaemon. */
		global_cwdevice->free(global_cwdevice);
		errno = 0;
		cwdaemon_errmsg("Sender has told me to end the connection");
		exit(EXIT_SUCCESS);

	case '6':
		/* Set uninterruptable (word mode). */
		request[0] = '\0';
		request_queue[0] = '\0';
		wordmode = 1;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Wordmode set");
		break;
	case '7':
		/* Set weighting of morse code dits and dashes.
		   Remember that cwdaemon uses values in range
		   -50/+50, but libcw accepts values in range
		   20/80. This is why you have the calculation
		   when calling cw_set_weighting(). */
		/* TODO: other options have current_* variable. Where
		   is current_weighting? */
		{
			int weighting = 0;
			if (cwdaemon_args_weighting(&weighting, request + 2)) {
				cw_set_weighting(weighting * 0.6 + CWDAEMON_MORSE_WEIGHTING_MAX);
				cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Weight: %d", weighting);
			}
		}
		break;
	case '8': {
		/* Set type of keying device. */
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Setting new keying device: %s", request + 2);
		cwdaemon_set_cwdevice(&global_cwdevice, request + 2);

		break;
	}
	case '9':
		/* Base port number.
		   TODO: why this is obsolete? */
		cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__, "Obsolete control data '9' (change network port), ignoring");
		break;
	case 'a':
		/* PTT keying on or off */
		if (cwdaemon_get_long(request + 2, &lv)) {
			break;
		}

		if (lv) {
			//global_cwdevice->ptt(global_cwdevice, ON);
			if (current_ptt_delay) {
				cwdaemon_set_ptt_on(global_cwdevice, "PTT (manual, delay) on");
			} else {
				cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "PTT (manual, immediate) on");
			}

			ptt_flag |= PTT_ACTIVE_MANUAL;
			cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag +PTT_ACTIVE_MANUAL (%02d, %d)", ptt_flag, __LINE__);

		} else if (ptt_flag & PTT_ACTIVE_MANUAL) {	/* only if manually activated */

			ptt_flag &= ~PTT_ACTIVE_MANUAL;
			cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag -PTT_ACTIVE_MANUAL (%02d, %d)", ptt_flag, __LINE__);

			if (!(ptt_flag & !PTT_ACTIVE_AUTO)) {	/* no PTT modifiers */

				if (request_queue[0] == '\0'/* no new text in the meantime */
				    && cw_get_tone_queue_length() <= 1) {

					cwdaemon_set_ptt_off(global_cwdevice, "PTT (manual, immediate) off");
				} else {
					/* still sending, cannot yet switch PTT off */
					ptt_flag |= PTT_ACTIVE_AUTO;	/* ensure auto-PTT active */
					cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag +PTT_ACTIVE_AUTO (%02d, %d)", ptt_flag, __LINE__);

					cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "reverting from PTT (manual) to PTT (auto) now");
				}
			}
		}
		break;
	case 'b':
		/* SSB way. */
#if defined(HAVE_LINUX_PPDEV_H) || defined(HAVE_DEV_PPBUS_PPI_H)
		if (cwdaemon_get_long(request + 2, &lv)) {
			break;
		}

		if (lv) {
			if (global_cwdevice->ssbway) {
				global_cwdevice->ssbway(global_cwdevice, SOUNDCARD);
				cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "SSB way set to SOUNDCARD", PACKAGE);
			} else {
				cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__, "SSB way to SOUNDCARD unimplemented");
			}
		} else {
			if (global_cwdevice->ssbway) {
				global_cwdevice->ssbway(global_cwdevice, MICROPHONE);
				cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "SSB way set to MIC");
			} else {
				cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__, "SSB way to MICROPHONE unimplemented");
			}
		}
#else
		cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__, "'SSB way' through parallel port unavailable (parallel port not configured).");
#endif
		break;
	case 'c':
		/* Tune for a number of seconds. */
		if (cwdaemon_get_long(request + 2, &lv)) {
			break;
		}

		if (lv <= 10) {
			/* TODO: why the limitation to 10 s? Is it enough? */
			cwdaemon_tune(lv);
		}
		break;
	case 'd':
		/* Set PTT delay (TOD, Turn On Delay).
		   The value is milliseconds. */
		if (!cwdaemon_args_pttdelay(&current_ptt_delay, request + 2)) {
			/* Why do we set here delay to MAX? What if lv
			   < 0 - shouldn't we set it to zero then? */
			current_ptt_delay = CWDAEMON_PTT_DELAY_MAX;
		}

		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "PTT delay(TOD): %d ms", current_ptt_delay);

		if (current_ptt_delay == 0) {
			cwdaemon_set_ptt_off(global_cwdevice, "ensure PTT off");
		}
		break;
	case 'e':
		/* Set band switch output on parport bits 9 (MSB), 8, 7, 2 (LSB). */
#if defined(HAVE_LINUX_PPDEV_H) || defined(HAVE_DEV_PPBUS_PPI_H)
		if (cwdaemon_get_long(request + 2, &lv)) {
			break;
		}

		if (lv >= 0 && lv <= 15) {
			cwdaemon_switch_band(global_cwdevice, lv);
		}
#else
		cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__, "Band switching through parallel port is unavailable (parallel port not configured).");
#endif
		break;
	case 'f': {
		/* Change sound system used by libcw. */
		/* FIXME: if "request+2" describes unavailable sound system,
		   cwdaemon fails to open the new sound system. Since
		   the old one is closed with cwdaemon_close_libcw_output(),
		   cwdaemon has no working sound system, and is unable to
		   play sound.

		   This can be fixed either by querying libcw if "request+2"
		   sound system is available, or by first trying to
		   open new sound system and then - on success -
		   closing the old one. In either case cwdaemon would
		   require some method to inform client about success
		   or failure to open new sound system.	*/
		if (cwdaemon_args_system(&current_audio_system, request + 2)) {
			/* Handle valid request for changing sound system. */
			cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Switching to sound system '%s'", cw_get_audio_system_label(current_audio_system));
			cwdaemon_close_libcw_output();

			if (!cwdaemon_open_libcw_output(current_audio_system)) {
				has_audio_output = true;
			} else {
				has_audio_output = false;
			}
		}
		break;
	}
	case 'g':
		/* Set volume of sound, in percents. */
		if (cwdaemon_args_volume(&current_morse_volume, request + 2)) {
			cw_set_volume(current_morse_volume);
		}
		break;
	case 'h':
		/* Data after '<ESC>h' is a text to be used as reply.
		   It shouldn't be echoed back to client immediately.

		   Instead, cwdaemon should wait for another request
		   (I assume that it will be a regular text to be
		   played), play it, and then send prepared reply back
		   to the client.  So this is a reply with delay. */

		/* 'request + 1' skips the leading <ESC>, but
		   preserves 'h'.  The 'h' is a part of reply text. If
		   the client didn't specify reply text, the 'h' will
		   be the only content of server's reply. */

		cwdaemon_prepare_reply(reply_buffer, request + 1, strlen(request + 1));
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Reply is ready, waiting for message from client (reply = '%s')", reply_buffer);
		/* cwdaemon will wait for queue-empty callback before
		   sending the reply. */
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

			if (current_morse_speed < CW_SPEED_MIN) {
				current_morse_speed = CW_SPEED_MIN;
			} else if (current_morse_speed > CW_SPEED_MAX) {
				current_morse_speed = CW_SPEED_MAX;
			} else {
				;
			}
			cw_set_send_speed(current_morse_speed);
			break;
		case '~':
			/* 2 dots time additional for the next char. The gap
			   is always reset after playing the char. */
			cw_set_gap(2);
			x++;
			break;
		case '^':
			/* Send echo to main program when CW playing is done. */
			*x = '\0';     /* Remove '^' and possible trailing garbage. */
			/* I'm guessing here that '^' can be found at
			   the end of request, and it means "echo text of current
			   request back to sender once you finish playing it". */
			cwdaemon_prepare_reply(reply_buffer, request, strlen(request));

			/* cwdaemon will wait for queue-empty callback
			   before sending the reply. */
			break;
		case '*':
			/* TODO: what's this? */
			*x = '+';
		default:
			cwdaemon_set_ptt_on(global_cwdevice, "PTT (auto) on");
			/* PTT is now in AUTO. It will be turned off on low
			   tone queue, in cwdaemon_tone_queue_low_callback(). */

			cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Morse = '%c'", *x);
			cw_send_character(*x);
			cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "Morse character '%c' has been queued in libcw", *x);
			x++;
			if (cw_get_gap() == 2) {
				if (*x == '^') {
					/* '^' is supposed to be the
					   last character in the
					   message, meaning that all
					   that was before it, should
					   be used as reply text. So
					   x++ will jump to ending
					   NUL */
					x++;
				} else {
					cw_set_gap(0);
				}
			}
			break;
		}
	}

	/* All characters processed, mark buffer as empty. */
	*request = '\0';

	return;
}





/**
   \brief Callback function for key state change

   Function passed to libcw, will be called every time a state of libcw's
   internal ("software") key changes, i.e. every time it starts or ends
   producing dit or dash.
   When the software key is closed (dit or dash), \p keystate is 1.
   Otherwise \p keystate is 0. Following the changes of \p keystate the
   function changes state of bit on output of its keying device.

   So it goes like this:
   input text in cwdaemon is split into characters ->
   characters in cwdaemon are sent to libcw ->
   characters in libcw are converted to dits/dashes ->
   dits/dashes are played by libcw, and at the same time libcw changes
           state of its software key ->
   libcw calls cwdaemon_keyingevent() on changes of software key ->
   cwdaemon_keyingevent() changes state of a bit on cwdaemon's keying device

   \param unused argument
   \param keystate - state of a key, as seen by libcw
*/
void cwdaemon_keyingevent(__attribute__((unused)) void *arg, int keystate)
{
	if (keystate == 1) {
		global_cwdevice->cw(global_cwdevice, ON);
	} else {
		global_cwdevice->cw(global_cwdevice, OFF);
	}

	inactivity_seconds = 0;

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "keying event %d", keystate);

	return;
}





/**
   \brief Callback routine called when tone queue is empty

   Callback routine registered with cw_register_tone_queue_low_callback(),
   will be called by libcw every time number of tones drops in queue below
   specific level.

   \param arg - unused argument
*/
void cwdaemon_tone_queue_low_callback(__attribute__((unused)) void *arg)
{
	cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "Low TQ callback start, ptt_flag=%02x", ptt_flag);

	if (ptt_flag == PTT_ACTIVE_AUTO
	    /* PTT is (most probably?) on, in purely automatic mode.
	       This means that as soon as there are no new chars to
	       play, we should turn PTT off. */

	    && request_queue[0] == '\0'
	    /* No new text has been queued in the meantime. */

	    && cw_get_tone_queue_length() <= tq_low_watermark) {
		/* TODO: check if this third condition is really necessary. */
		/* Originally twas 'cw_get_tone_queue_length() <= 1',
		   I'm guessing that '1' here was the same '1' as the
		   third argument to cw_register_tone_queue_low_callback().
		   Feel free to correct me ;) */


		cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "Low TQ callback branch 1, ptt_flag = %02d", ptt_flag);

		cwdaemon_set_ptt_off(global_cwdevice, "PTT (auto) off");

	} else if (ptt_flag & PTT_ACTIVE_ECHO) {
		/* PTT_ACTIVE_ECHO: client has used special request to
		   indicate that it is waiting for reply (echo) from
		   the server (i.e. cwdaemon) after the server plays
		   all characters. */

		cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "Low TQ callback branch 2, ptt_flag = %02d", ptt_flag);

		/* Since echo is being sent, we can turn the flag off.
		   For some reason cwdaemon works better when we turn the
		   flag off before sending the reply, rather than turning
		   if after sending the reply. */
		ptt_flag &= ~PTT_ACTIVE_ECHO;
		cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag -PTT_ACTIVE_ECHO (%02d, %d)", ptt_flag, __LINE__);


		cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "Echoing '%s' back to client", reply_buffer);
		strcat(reply_buffer, "\r\n"); /* Ensure exactly one CRLF */
		cwdaemon_sendto(reply_buffer);
		/* If this line is uncommented, the callback erases a valid
		   reply that should be sent back to client. Commenting the
		   line fixes the problem, and doesn't seem to introduce
		   any new ones.
		   TODO: investigate the original problem of erasing a valid
		   reply. */
		/* reply_buffer[0] = '\0'; */


		/* wait a bit more since we expect to get more text to send

		   TODO: the comment above is a bit unclear.  Perhaps
		   it means that we have dealt with escape request
		   requesting an echo, and now there may be a second
		   request (following the escape request) that still
		   needs to be played ("more text to send").

		   If so, we need to call the callback again, so that
		   it can check if text buffer is non-empty and if
		   tone queue is non-empty. We call the callback again
		   indirectly, by simulating almost empty tone queue
		   (i.e. by adding only two tones to tone queue).

		   I wonder why we don't call the callback directly,
		   maybe it has something to do with avoiding
		   recursion? */
		if (ptt_flag == PTT_ACTIVE_AUTO) {
			cw_queue_tone(1, 0); /* ensure Q-empty condition again */
			cw_queue_tone(1, 0); /* when trailing gap also 'sent' */
		}
	} else {
		/* TODO: how to correctly handle this case?
		   Should we do something? */
		cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "Low TQ callback branch 3, ptt_flag = %02d", ptt_flag);
	}

	cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "Low TQ callback end");

	return;

}



static const char   *cwdaemon_args_short = "d:hniI:f:p:P:s:t:T:v:Vw:x:";

static struct option cwdaemon_args_long[] = {
	{ "cwdevice",    required_argument,       0, 0},  /* Keying device. */
	{ "nofork",      no_argument,             0, 0},  /* Don't fork. */
	{ "port",        required_argument,       0, 0},  /* Network port number. */
#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
	{ "priority",    required_argument,       0, 0},  /* Process priority. */
#endif
	{ "wpm",         required_argument,       0, 0},  /* Sending speed. */
	{ "pttdelay",    required_argument,       0, 0},  /* PTT delay. */
	{ "volume",      required_argument,       0, 0},  /* Sound volume. */
	{ "version",     no_argument,             0, 0},  /* Program's version. */
	{ "weighting",   required_argument,       0, 0},  /* CW weight. */
	{ "tone",        required_argument,       0, 0},  /* CW tone. */
	{ "verbosity",   required_argument,       0, 0},  /* Verbosity of cwdaemon's debug messages. */
	{ "libcwflags",  required_argument,       0, 0},  /* libcw's debug flags. */
	{ "debugfile",   required_argument,       0, 0},  /* Path to output debug file. */
	{ "system",      required_argument,       0, 0},  /* Audio system. */
	{ "help",        no_argument,             0, 0},  /* Print help text and exit. */

	{ 0,             0,                       0, 0} };




void cwdaemon_args_process_long(int argc, char *argv[])
{
	int option_index = 0;

	int c = 0;

	while ((c = getopt_long(argc, argv, cwdaemon_args_short, cwdaemon_args_long, &option_index)) != -1) {
		if (c == 0) {
			fprintf(stderr, "INFO: long option %s%s%s\n",
				cwdaemon_args_long[option_index].name,
				optarg ? "=" : "",
				optarg ? optarg : "");

			const char *optname = cwdaemon_args_long[option_index].name;

			if (!strcmp(optname, "cwdevice")) {
				if (!cwdaemon_args_cwdevice(optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "nofork")) {
				cwdaemon_args_nofork();

			} else if (!strcmp(optname, "port")) {
				if (!cwdaemon_args_port(optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "priority")) {
				if (!cwdaemon_args_priority(&process_priority, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "wpm")) {
				if (!cwdaemon_args_wpm(&default_morse_speed, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "pttdelay")) {
				if (!cwdaemon_args_pttdelay(&default_ptt_delay, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "volume")) {
				if (!cwdaemon_args_volume(&default_morse_volume, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "version")) {
				cwdaemon_args_version();
				exit(EXIT_SUCCESS);

			} else if (!strcmp(optname, "weighting")) {
				if (!cwdaemon_args_weighting(&default_weighting, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "tone")) {
				if (!cwdaemon_args_tone(&default_morse_tone, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "verbosity")) {
				if (!cwdaemon_args_set_verbosity(optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "libcwflags")) {
				if (!cwdaemon_args_libcwflags(optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "debugfile")) {
				if (!cwdaemon_args_debugfile(optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "system")) {
				if (!cwdaemon_args_system(&default_audio_system, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else {
				cwdaemon_args_help();
				exit(EXIT_SUCCESS);
			}
		} else {
			cwdaemon_args_process_short(c, optarg);
		}
	}

	return;
}


void cwdaemon_args_process_short(int c, const char *optarg)
{
	switch (c) {
	case ':':
	case '?':
	case 'h':
		cwdaemon_args_help();
		exit(EXIT_SUCCESS);
	case 'd':
		if (!cwdaemon_args_cwdevice(optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'n':
		cwdaemon_args_nofork();
		break;
	case 'p':
		if (!cwdaemon_args_port(optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
	case 'P':
		if (!cwdaemon_args_priority(&process_priority, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
#endif
	case 's':
		if (!cwdaemon_args_wpm(&default_morse_speed, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 't':
		if (!cwdaemon_args_pttdelay(&default_ptt_delay, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'v':
		if (!cwdaemon_args_volume(&default_morse_volume, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'V':
		cwdaemon_args_version();
		exit(EXIT_SUCCESS);
	case 'w':
		if (!cwdaemon_args_weighting(&default_weighting, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'T':
		if (!cwdaemon_args_tone(&default_morse_tone, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'i':
		cwdaemon_args_inc_verbosity();
		break;
	case 'I':
		if (!cwdaemon_args_libcwflags(optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'f':
		if (!cwdaemon_args_debugfile(optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'x':
		if (!cwdaemon_args_system(&default_audio_system, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	}

	return;
}


bool cwdaemon_args_cwdevice(const char *optarg)
{
	if (cwdaemon_set_cwdevice(&global_cwdevice, optarg) == -1) {
		return false;
	} else {
		return true;
	}
}

void cwdaemon_args_nofork(void)
{
	if (forking) {
		printf("%s: Not forking...\n", PACKAGE);
		forking = 0;
	}
	return;
}


bool cwdaemon_args_port(const char *optarg)
{
	long lv = 0;
	if (cwdaemon_get_long(optarg, &lv) || lv < 1024 || lv > 65536) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid value of port number: \"%s\"", optarg);
		return false;
	} else {
		port = lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested port number = %ld", port);
		return true;
	}
}


bool cwdaemon_args_priority(int *priority, const char *optarg)
{
	long lv = 0;
	if (cwdaemon_get_long(optarg, &lv) || lv < -20 || lv > 20) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid value of priority: \"%s\" (should be between -20 and 20 inclusive)",
			       optarg);
		return false;
	} else {
		*priority = (int) lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested process priority = %ld", *priority);
		return true;
	}
}


bool cwdaemon_args_wpm(int *wpm, const char *optarg)
{
	long lv = 0;
	if (cwdaemon_get_long(optarg, &lv) || lv < CW_SPEED_MIN || lv > CW_SPEED_MAX) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__ ,
			       "invalid value of speed: \"%s\" (should be between %d and %d inclusive)",
			       optarg, CW_SPEED_MIN, CW_SPEED_MAX);
		return false;
	} else {
		*wpm = (int) lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested wpm = %d", *wpm);
		return true;
	}
}


bool cwdaemon_args_pttdelay(int *delay, const char *optarg)
{
	long lv = 0;
	if (cwdaemon_get_long(optarg, &lv) || lv < CWDAEMON_PTT_DELAY_MIN || lv > CWDAEMON_PTT_DELAY_MAX) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid value of PTT delay: \"%s\" (should be between %d and %d [ms] inclusive)",
			       optarg, CWDAEMON_PTT_DELAY_MIN, CWDAEMON_PTT_DELAY_MAX);
		return false;
	} else {
		*delay = (int) lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested PTT delay: %ld", *delay);
		return true;
	}
}


bool cwdaemon_args_volume(int *volume, const char *optarg)
{
	long lv = 0;
	if (cwdaemon_get_long(optarg, &lv) || lv < CW_VOLUME_MIN || lv > CW_VOLUME_MAX) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid value of volume: \"%s\" (should be between %d and %d [%%] inclusive)",
			       optarg, CW_VOLUME_MIN, CW_VOLUME_MAX);
		return false;
	} else {
		*volume = (int) lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested volume: %d", *volume);
		return true;
	}
}


void cwdaemon_args_version(void)
{
	printf("%s version %s\n", PACKAGE, VERSION);
	return;
}


bool cwdaemon_args_weighting(int *weighting, const char *optarg)
{
	long lv = 0;
	if (cwdaemon_get_long(optarg, &lv) || lv < CWDAEMON_MORSE_WEIGHTING_MIN || lv > CWDAEMON_MORSE_WEIGHTING_MAX) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid value of weighting: \"%s\" (should be between %d and %d inclusive)",
			       optarg, CWDAEMON_MORSE_WEIGHTING_MIN, CWDAEMON_MORSE_WEIGHTING_MAX);
		return false;
	} else {
		*weighting = (int) lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested weighting: %ld", *weighting);
		return true;
	}
}


bool cwdaemon_args_tone(int *tone, const char *optarg)
{
	long lv = 0;
	if (cwdaemon_get_long(optarg, &lv) || lv < CW_FREQUENCY_MIN || lv > CW_FREQUENCY_MAX) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid value of tone: \"%s\" (should be between %d and %d [Hz] inclusive)",
			       optarg, CW_FREQUENCY_MIN, CW_FREQUENCY_MAX);
		return false;
	} else {
		*tone = (int) lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested tone: %ld", *tone);
		return true;
	}
}


void cwdaemon_args_inc_verbosity(void)
{
	if (cwdaemon_verbosity < CWDAEMON_VERBOSITY_D) {
		cwdaemon_verbosity++;
	}
	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
		       "requested verbosity level: %s", cwdaemon_verbosity_labels[cwdaemon_verbosity]);
	return;
}


bool cwdaemon_args_set_verbosity(const char *optarg)
{
	if (!strcmp(optarg, "n") || !strcmp(optarg, "N")) {
		cwdaemon_verbosity = CWDAEMON_VERBOSITY_N;
	} else if (!strcmp(optarg, "e") || !strcmp(optarg, "E")) {
		cwdaemon_verbosity = CWDAEMON_VERBOSITY_E;
	} else if (!strcmp(optarg, "w") || !strcmp(optarg, "W")) {
		cwdaemon_verbosity = CWDAEMON_VERBOSITY_W;
	} else if (!strcmp(optarg, "i") || !strcmp(optarg, "I")) {
		cwdaemon_verbosity = CWDAEMON_VERBOSITY_I;
	} else if (!strcmp(optarg, "d") || !strcmp(optarg, "D")) {
		cwdaemon_verbosity = CWDAEMON_VERBOSITY_D;
	} else {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid value of verbosity level: \"%s\"", optarg);
		return false;
	}

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
		       "requested verbosity level = %s", cwdaemon_verbosity_labels[cwdaemon_verbosity]);
	return true;
}


bool cwdaemon_args_libcwflags(const char *optarg)
{
	long lv = 0;
	if (cwdaemon_get_long(optarg, &lv)) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid value of debug flags: \"%s\" (should be numeric value)", optarg);
		libcw_debug_flags = 0;

		return false;
	} else {
		libcw_debug_flags = lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested libcw debug flags = %ld", libcw_debug_flags);
		return true;
	}
}


bool cwdaemon_args_debugfile(const char *optarg)
{
	cwdaemon_debug_f_path = strdup(optarg);
	if (!cwdaemon_debug_f_path) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "failed to copy path to debug file \"%s\"", optarg);
		return false;
	} else {
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested debug file path = \"%s\"", cwdaemon_debug_f_path);
		return true;
	}
}


bool cwdaemon_args_system(int *system, const char *optarg)
{
	if (!strncmp(optarg, "n", 1)) {
		*system = CW_AUDIO_NULL;
	} else if (!strncmp(optarg, "c", 1)) {
		*system = CW_AUDIO_CONSOLE;
	} else if (!strncmp(optarg, "s", 1)) {
		*system = CW_AUDIO_SOUNDCARD;
	} else if (!strncmp(optarg, "a", 1)) {
		*system = CW_AUDIO_ALSA;
	} else if (!strncmp(optarg, "p", 1)) {
		*system = CW_AUDIO_PA;
	} else if (!strncmp(optarg, "o", 1)) {
		*system = CW_AUDIO_OSS;
	} else {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid sound system: \"%s\" (use c(onsole), o(ss), a(lsa), p(ulseaudio), n(one - no audio), or s(oundcard - autoselect from OSS/ALSA/PulseAudio))",
			       optarg);
		return false;
	}

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
		       "requested sound system: \"%s\"", optarg);
	return true;
}





/**
   \brief Get and parse command line options

   Scan program's arguments, check for command line options, parse them.

   \param argc - main()'s argc argument
   \param argv - main()'s argv argument
*/
void cwdaemon_args_parse(int argc, char *argv[])
{
#if defined(HAVE_GETOPT_H)
	cwdaemon_args_process_long(argc, argv);
#else
	int p;

	while ((p = getopt(argc, argv, cwdaemon_args_short)) != -1) {
		long lv;

		cwdaemon_args_process_short(p, optarg);
	}
#endif
	return;
}





/**
   \brief Configure file descriptor for debug output
 */
void cwdaemon_debug_open(void)
{
	if (cwdaemon_debug_f_path) {
		/* Don't write to stdout (regardless if we are forking
		   or not), write to disk file. */
		cwdaemon_debug_f = fopen(cwdaemon_debug_f_path, "w+");
		if (!cwdaemon_debug_f) {
			fprintf(stderr, "%s: failed to open output file \"%s\"\n", PACKAGE, cwdaemon_debug_f_path);
			return;
		}
	} else {
		if (forking) {
			cwdaemon_debug_f = NULL;
		} else {
			cwdaemon_debug_f = stdout;
		}
	}

	return;
}





void cwdaemon_args_help(void)
{
	printf("Usage: %s [option]...\n", PACKAGE);
	printf("Long options may not be supported on your system.\n\n");

	printf("-h, --help\n");
	printf("        Display this help and exit.\n");
	printf("-V, --version\n");
	printf("        Output version information and exit.\n");

	printf("-d, --cwdevice <device>\n");
	printf("        Use a different device.\n");
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

	printf("-n, --nofork\n");
	printf("        Do not fork. Print debug information to stdout.\n");
	printf("-p, --port <port>\n");
	printf("        Use a different UDP port number (> 1023, default = %d).\n", CWDAEMON_NETWORK_PORT_DEFAULT);
#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
	printf("-P, --priority <priority>\n");
	printf("        Set program's priority (-20 ... 20, default = 0).\n");
#endif
	printf("-s, --wpm <speed>\n");
	printf("        Set morse speed (%d ... %d wpm, default = %d).\n",
	       CW_SPEED_MIN, CW_SPEED_MAX, CWDAEMON_MORSE_SPEED_DEFAULT);
	printf("-t, --pttdelay <time>\n");
	printf("        Set PTT delay (%d - %d ms, default = %d).\n",
	       CWDAEMON_PTT_DELAY_MIN, CWDAEMON_PTT_DELAY_MAX, CWDAEMON_PTT_DELAY_DEFAULT);
	printf("-x, --system <sound system>\n");
	printf("        Use a specific sound system:\n");
	printf("        c = console buzzer (default)\n");
	printf("        o = OSS\n");
	printf("        a = ALSA\n");
	printf("        p = PulseAudio\n");
	printf("        n = none (no audio)\n");
	printf("        s = soundcard (autoselect from OSS/ALSA/PulseAudio)\n");
	printf("-v, --volume <volume>\n");
	printf("        Set volume for soundcard output (%d%% - %d%%, default = %d%%).\n",
	       CW_VOLUME_MIN, CW_VOLUME_MAX, CWDAEMON_MORSE_VOLUME_DEFAULT);
	printf("-w, --weighting <weight>\n");
	printf("        Set weighting (%d - %d, default = %d).\n",
	       CWDAEMON_MORSE_WEIGHTING_MIN, CWDAEMON_MORSE_WEIGHTING_MAX, CWDAEMON_MORSE_WEIGHTING_DEFAULT);
	printf("-T, --tone <tone>\n");
	printf("        Set initial tone to 'tone' (%d - %d Hz, default: %d).\n",
	       CW_FREQUENCY_MIN, CW_FREQUENCY_MAX, CWDAEMON_MORSE_TONE_DEFAULT);
	printf("-i\n");
	printf("        Increase verbosity of debug messages printed by cwademon.\n");
	printf("        Repeat for even more verbosity.\n");
	printf("--verbosity <level>\n");
	printf("        Set verbosity level of messages printed by cwdaemon.\n");
	printf("        Recognized values:\n");
	printf("        n = none (default)\n");
	printf("        e = errors\n");
	printf("        w = warnings\n");
	printf("        i = information\n");
	printf("        d = debug (details)\n");
	printf("-I, --libcwflags <flags>\n");
	printf("        Numeric value of debug flags to be passed to libcw.\n");
	printf("-f, --debugfile <path>\n");
	printf("        Print debug information to file instead of stdout.\n");
	printf("        Also works when %s has forked.\n", PACKAGE);
	printf("\n");

	return;
}





/* main program: fork, open network connection and go into an endless loop
   waiting for something to happen on the UDP port */
int main(int argc, char *argv[])
{
	//cw_debug_set_flags(&cw_debug_object_dev, CW_DEBUG_GENERATOR | CW_DEBUG_SOUND_SYSTEM);
	//(&cw_debug_object_dev)->level = CW_DEBUG_DEBUG;

	/* Until a call to cwdaemon_debug_open() is made, and debug
	   output is configured according to command line switches,
	   use the default "stdout" file. */
	cwdaemon_debug_f = stdout;

	atexit(cwdaemon_free_cwdevice_descriptions);
	if (cwdaemon_set_default_cwdevice_descriptions() == -1) {
		exit(EXIT_FAILURE);
	}

	cwdaemon_args_parse(argc, argv);

	atexit(cwdaemon_debug_close);
	cwdaemon_debug_open();


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

		// cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Child process reporting.");
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
		fprintf(stdout, "Press ^C to quit\n");
		signal(SIGINT, cwdaemon_catch_sigint);
	}

	if (cwdaemon_initialize_socket() == -1) {
		exit(EXIT_FAILURE);
	}

#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
	if (process_priority != 0) {
		if (setpriority(PRIO_PROCESS, getpid(), process_priority) < 0) {
			cwdaemon_errmsg("Setting process priority: \"%s\"", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
#endif

	/* Initialize libcw (and other things) here, this late, to
	   be sure that libcw has been initialized (and is still used)
	   by child process. Parent process exits after forking, and so
	   libcw is closed in parent process. We need it in child process,
	   so here it is. */
	cwdaemon_reset_almost_all();
	atexit(cwdaemon_close_libcw_output);

	if (libcw_debug_flags) { /* debugging libcw as well */
		cw_set_debug_flags(libcw_debug_flags);
	}


	cw_register_keying_callback(cwdaemon_keyingevent, NULL);
	cw_register_tone_queue_low_callback(cwdaemon_tone_queue_low_callback, NULL, tq_low_watermark);


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
		/* fd_count = select(socket_descriptor + 1, &readfd, NULL, NULL, NULL); */
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





/**
   \brief Set up initial values of cw keying device names

   Allocate strings with device names, assign them to appropriate
   cwdevice_xyz.desc values.

   The strings can be later deallocated with
   cwdaemon_free_cwdevice_descriptions().

   \return 0
*/
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





/**
   \brief Deallocate strings with cw keying device names

   Function frees memory previously allocated with
   cwdaemon_free_cwdevice_descriptions().
*/
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





/**
   \brief Assign correct device type to given device variable

   A device setup function. The function takes device name (description)
   \p desc, guesses device type (parport/tty/null), and assigns the guessed
   device to given \p device.

   Function assigns to \p device a pointer to global variable, there is
   no need to free memory associated with the variable itself.
   The function copies \p desc to 'global device'->desc , you will have to
   deallocate the copied desc with cwdaemon_free_cwdevice_descriptions().

   The function can be called with \p device to which some other device
   has been already assigned.

   \param device - pointer to device variable - guessed device will be

   \return -1 if \p desc describes invalid device, or if memory allocation error happens
   \return 0 otherwise
*/
int cwdaemon_set_cwdevice(cwdevice **device, const char *desc)
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
		*device = NULL;
	}

	if (!*device) {
		printf("%s: bad keyer device: %s\n", PACKAGE, desc);
		return -1;
	}

	if ((*device)->desc) {
		free((*device)->desc);
	}
	(*device)->desc = strdup(desc);
	if (!(*device)->desc) {
		printf("%s: memory allocation error\n", PACKAGE);
		return -1;
	}

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Keying device used: %s", (*device)->desc);
	(*device)->init(*device, fd);

	return 0;

}





/**
   \brief Initialize network variables

   Initialize network socket and other network variables.

   \return -1 on failure
   \return 0 on success
*/
int cwdaemon_initialize_socket(void)
{
	memset(&request_addr, '\0', sizeof (request_addr));
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
	if ((tv->tv_usec += usecs) >= CWDAEMON_USECS_PER_SEC)
	{
		tv->tv_sec += tv->tv_usec / CWDAEMON_USECS_PER_SEC;
		tv->tv_usec %= CWDAEMON_USECS_PER_SEC;
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
