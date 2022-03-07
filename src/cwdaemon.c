/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
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

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#define _POSIX_C_SOURCE 200809L /* nanosleep(), strdup() */
#define _GNU_SOURCE /* getopt_long() */

#include "config.h"

#include <stdbool.h>

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
#if HAVE_ARPA_INET_H
# include <arpa/inet.h>
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

#include <stdint.h> /* uint32_t */

#include <libcw.h>
#include <libcw_debug.h>
#include "cwdaemon.h"
#include "help.h"
#include "socket.h"




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



   cwdaemon can be configured either through command line arguments on
   start of the daemon, or through requests (escaped requests) sent
   over network.

<verbatim>
   Feature               command line argument     escaped request
   ---------------------------------------------------------------
   help                  -h, --help                N/A
   version               -V, --version             N/A
   keying device         -d, --cwdevice            8
   don't fork daemon     -n, --nofork              N/A
   driver option         -o, --options             N/A
   network port          -p, --port                9 (obsolete)
   process priority      -P, --priority            N/A
   Morse speed (wpm)     -s, --wpm                 2
   PTT delay             -t, --pttdelay            d
   PTT keying on/off     N/A                       a
   sound system          -x, --system              f
   sound volume          -v, --volume              g
   Morse weighting       -w, --weighting           7
   sound tone            -T, --tone                3
   debug verbosity       -i                        N/A
   debug verbosity       -y, --verbosity           N/A
   libcw debug flags     -I, --libcwflags          N/A
   debug output          -f, --debugfile           N/A

   reset parameters      N/A                       0
   abort message         N/A                       4
   exit daemon           N/A                       5
   set word mode         N/A                       6
   set SSB way           N/A                       b
   tune                  N/A                       c
   band switch           N/A                       e

   </verbatim>


*/


/* This flag is necessary until I'm done with writing a good test for the
   ticket. It may be necessary even afterwards, just to be able to quickly
   restore faulty behaviour and run a test against it. */
#define CWDAEMON_GITHUB_ISSUE_6_FIXED


/* cwdaemon constants. */
#define CWDAEMON_AUDIO_SYSTEM_DEFAULT      CW_AUDIO_CONSOLE /* Console buzzer, from libcw.h. */
#define CWDAEMON_VERBOSITY_DEFAULT     CWDAEMON_VERBOSITY_W /* Threshold of verbosity of debug strings. */

#define CWDAEMON_USECS_PER_MSEC         1000 /* Just to avoid magic numbers. */
#define CWDAEMON_USECS_PER_SEC       1000000 /* Just to avoid magic numbers. */
#define CWDAEMON_MESSAGE_SIZE_MAX        256 /* Maximal size of single message. */
#define CWDAEMON_REQUEST_QUEUE_SIZE_MAX 4000 /* Maximal size of common buffer/fifo where requests may be pushed to. */

#define CWDAEMON_TUNE_SECONDS_MAX  10 /* Maximal time of tuning. TODO: why the limitation to 10 s? Is it enough? */


/* For developer's debugging purposes. */
//extern cw_debug_t cw_debug_object_dev;


/* Default values of parameters, may be modified only through
   command line arguments passed to cwdaemon.

   After setting these variables with values passed in command line,
   these become the default state of cwdaemon.  Values of default_*
   will be used when resetting libcw and cwdaemon to initial state. */
static int default_morse_speed  = CWDAEMON_MORSE_SPEED_DEFAULT;
static int default_morse_tone   = CWDAEMON_MORSE_TONE_DEFAULT;
static int default_morse_volume = CWDAEMON_MORSE_VOLUME_DEFAULT;
static int default_ptt_delay    = CWDAEMON_PTT_DELAY_DEFAULT;
static int default_audio_system = CWDAEMON_AUDIO_SYSTEM_DEFAULT;
static int default_weighting    = CWDAEMON_MORSE_WEIGHTING_DEFAULT;
static int default_verbosity    = CWDAEMON_VERBOSITY_DEFAULT;


/* Actual values of parameters, used to control ongoing operation of
   cwdaemon+libcw. These values can be modified through requests
   received from socket in cwdaemon_receive(). */
static int current_morse_speed  = CWDAEMON_MORSE_SPEED_DEFAULT;
static int current_morse_tone   = CWDAEMON_MORSE_TONE_DEFAULT;
static int current_morse_volume = CWDAEMON_MORSE_VOLUME_DEFAULT;
static int current_ptt_delay    = CWDAEMON_PTT_DELAY_DEFAULT;
static int current_audio_system = CWDAEMON_AUDIO_SYSTEM_DEFAULT;
static int current_weighting    = CWDAEMON_MORSE_WEIGHTING_DEFAULT;
static int current_verbosity    = CWDAEMON_VERBOSITY_DEFAULT;

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

   This is a flag telling cwdaemon if audio output is available or not.

   TODO: the variable is almost unused. Start using it.

   TODO: decide on terminology: "audio system" or "sound system". */
static bool has_audio_output = false;




/* Default UDP port we listen on. Can be changed only through command
   line switch.

   There is a code path suggesting that it was possible to change the
   port using network request, but now this code path is marked as
   "obsolete".
 */
static int g_network_port = CWDAEMON_NETWORK_PORT_DEFAULT;


static char reply_buffer[CWDAEMON_MESSAGE_SIZE_MAX];


static cwdaemon_t g_cwdaemon = { .socket_descriptor = -1 };




/* Debug variables. */

/* cwdaemon may print debug strings to a disc file instead of
   stdout. */
static FILE *cwdaemon_debug_f = NULL;
static char *cwdaemon_debug_f_path = NULL;

/* This table is addressed with values defined in "enum
   cwdaemon_verbosity" (src/cwdaemon.h). */
static const char *cwdaemon_verbosity_labels[] = {
	"NN",    /* None - obviously this label will never be used. */
	"EE",    /* Error. */
	"WW",    /* Warning. */
	"II",    /* Information. */
	"DD" };  /* Debug. */

/* An integer that is a result of ORing libcw's debug flags. See
   libcw.h (or is it libcw_debug.h?) for numeric values of the
   flags. */
static long int libcw_debug_flags = 0;



/* Various variables. */
static int wordmode = 0;               /* Start in character mode. */
static int forking = 1;                /* We fork by default. */
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
   reply (TODO: why it shouldn't?).

   This flag is set whenever client sends request for sending back a
   reply (i.e. either <ESC>h request, or caret request).

   This flag is re-set whenever such reply is sent (to be more
   precise: after playing a requested text, but just before sending to
   the client the requested reply). */
#define PTT_ACTIVE_ECHO		0x04



void cwdaemon_set_ptt_on(cwdevice * dev, const char *info);
void cwdaemon_set_ptt_off(cwdevice * dev, const char *info);
void cwdaemon_switch_band(cwdevice * dev, unsigned int band);

void cwdaemon_play_request(char *request);

void cwdaemon_tune(uint32_t seconds);
void cwdaemon_keyingevent(void * arg, int keystate);
void cwdaemon_prepare_reply(cwdaemon_t * cwdaemon, char *reply, const char *request, size_t n);
void cwdaemon_tone_queue_low_callback(void *arg);


void cwdaemon_close_socket_wrapper(void);
int  cwdaemon_receive(void);
void cwdaemon_handle_escaped_request(char *request);

void cwdaemon_reset_almost_all(cwdevice * dev);

/* Functions managing cwdevices. */
bool cwdaemon_cwdevices_init(void);
void cwdaemon_cwdevices_free(void);
void cwdaemon_cwdevice_init(void);
bool cwdaemon_cwdevice_set(cwdevice **device, const char *desc);
void cwdaemon_cwdevice_free(void);

/* Functions managing libcw output. */
bool cwdaemon_open_libcw_output(int audio_system);
void cwdaemon_close_libcw_output(void);
void cwdaemon_reset_libcw_output(void);

/* Utility functions. */
bool cwdaemon_get_long(const char *buf, long *lvp);
void cwdaemon_udelay(unsigned long us);



static void cwdaemon_args_parse(int argc, char *argv[]);
static void cwdaemon_args_process_short(int c, const char *optarg);
static void cwdaemon_args_process_long(int argc, char *argv[]);

/* These two are called only in code handling command line options. No
   request can prompt cwdaemon to inform about version or to fork.

   While it would make some sense to make request to get version of
   cwdaemon while it is already running, it is impossible to request
   non-forking of daemon that - after parsing command line options -
   has already forked. */
static void cwdaemon_params_version(void);
static void cwdaemon_params_nofork(void);

static bool cwdaemon_params_cwdevice(const char *optarg);
static bool cwdaemon_params_network_port(const char *optarg, int * port);
static bool cwdaemon_params_priority(int *priority, const char *optarg);
static bool cwdaemon_params_wpm(int *wpm, const char *optarg);
static bool cwdaemon_params_tune(uint32_t *seconds, const char *optarg);
static int  cwdaemon_params_pttdelay(int *delay, const char *optarg);
static bool cwdaemon_params_volume(int *volume, const char *optarg);
static bool cwdaemon_params_weighting(int *weighting, const char *optarg);
static bool cwdaemon_params_tone(int *tone, const char *optarg);
static void cwdaemon_params_inc_verbosity(int *verbosity);
static bool cwdaemon_params_set_verbosity(int *verbosity, const char *optarg);
static bool cwdaemon_params_libcwflags(const char *optarg);
static bool cwdaemon_params_debugfile(const char *optarg);
static bool cwdaemon_params_system(int *system, const char *optarg);
static bool cwdaemon_params_ptt_on_off(const char *optarg);
static bool cwdaemon_params_options(cwdevice * dev, const char *optarg);



static void cwdaemon_debug_open(void);
static void cwdaemon_debug_close(void);

/* Auto, manual, echo. */
static char cwdaemon_debug_ptt_flag[3 + 1];
static const char *cwdaemon_debug_ptt_flags(void);

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
	.optparse   = ttys_optparse,
	.optvalidate = ttys_optvalidate,
	.cookie	    = NULL,
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
	.optparse   = NULL,
	.optvalidate = NULL,
	.cookie	    = NULL,
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
	.optparse   = NULL,
	.optvalidate = NULL,
	.cookie	    = NULL,
	.fd         = 0,
	.desc       = NULL
};
#endif



/* Selected keying device:
   serial port (cwdevice_ttys) || parallel port (cwdevice_lp) || null (cwdevice_null).
   It should be configured with cwdaemon_cwdevice_set(). */
/* FIXME: if no device is specified in command line, and no physical
   device is available, the global_cwdevice is NULL, which causes the
   program to break. */
static cwdevice *global_cwdevice = NULL;





/* catch ^C when running in foreground */
RETSIGTYPE cwdaemon_catch_sigint(__attribute__((unused)) int signal)
{
	printf("%s: Exiting\n", PACKAGE);
	exit(EXIT_SUCCESS);
}





/**
   \brief Print error string to the console or syslog

   Function checks if cwdaemon has forked, and prints given error string
   to stdout (if cwdaemon hasn't forked) or to syslog (if cwdaemon has
   forked).

   Function accepts printf-line formatting string as first argument, and
   a set of optional arguments to be inserted into the formatting string.

   \param format - first part of an error string, a formatting string
*/
void cwdaemon_errmsg(const char *format, ...)
{
	va_list ap;
	char s[1025];

	va_start(ap, format);
	vsnprintf(s, 1024, format, ap);
	va_end(ap);

	if (forking) {
		syslog(LOG_ERR, "%s\n", s);
	} else {
		printf("%s: %s failed: \"%s\"\n", PACKAGE, s, strerror(errno));
		fflush(stdout);
	}

	return;
}





/**
   \brief Print debug string to debug file

   Function decides if given \p verbosity level is sufficient to print
   given \p format debug string, and prints it to predefined file. If
   current global verbosity level is "None", no information will be
   printed.

   Currently \p verbosity can have one of values defined in enum
   cwdaemon_verbosity.

   Function accepts printf-line formatting string as last named
   argument (\p format), and a set of optional arguments to be
   inserted into the formatting string.

   \param verbosity - verbosity level of given debug string
   \param format - formatting string of a debug string being printed
*/
void cwdaemon_debug(int verbosity, __attribute__((unused)) const char *func, __attribute__((unused)) int line, const char *format, ...)
{
	if (current_verbosity > CWDAEMON_VERBOSITY_N
	    && verbosity <= current_verbosity
	    && cwdaemon_debug_f) {

		va_list ap;
		char s[1024 + 1];
		va_start(ap, format);
		vsnprintf(s, 1024, format, ap);
		va_end(ap);

		fprintf(cwdaemon_debug_f, "%s:%s: %s\n", PACKAGE, cwdaemon_verbosity_labels[verbosity], s);
		// fprintf(cwdaemon_debug_f, "cwdaemon:        %s(): %d\n", func, line);
		fflush(cwdaemon_debug_f);
	}

	return;
}




void cwdaemon_debug_close(void)
{
	if (cwdaemon_debug_f
	    && cwdaemon_debug_f != stdout
	    && cwdaemon_debug_f != stderr) {

		fclose(cwdaemon_debug_f);
		cwdaemon_debug_f = NULL;
	}

	return;
}





const char *cwdaemon_debug_ptt_flags(void)
{

	if (ptt_flag & PTT_ACTIVE_AUTO) {
		cwdaemon_debug_ptt_flag[0] = 'A';
	} else {
		cwdaemon_debug_ptt_flag[0] = 'a';
	}

	if (ptt_flag & PTT_ACTIVE_MANUAL) {
		cwdaemon_debug_ptt_flag[1] = 'M';
	} else {
		cwdaemon_debug_ptt_flag[1] = 'm';
	}

	if (ptt_flag & PTT_ACTIVE_ECHO) {
		cwdaemon_debug_ptt_flag[2] = 'E';
	} else {
		cwdaemon_debug_ptt_flag[2] = 'e';
	}

	cwdaemon_debug_ptt_flag[3] = '\0';

	return cwdaemon_debug_ptt_flag;
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

   \brief dev device to use to switch band
   \brief band band number to switch to (a hex number)
*/
#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
void cwdaemon_switch_band(cwdevice * dev, unsigned int band)
{
	unsigned int bit_pattern = (band & 0x01) | ((band & 0x0e) << 4);
	if (dev->switchband) {
		dev->switchband(dev, bit_pattern);
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "set band switch to %x", band);
	} else {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__, "band switch output not implemented");
	}

	return;
}
#endif





/**
   \brief Switch PTT on

   \param dev current keying device
   \param info debug information displayed when performing the switching
*/
void cwdaemon_set_ptt_on(cwdevice * dev, const char *info)
{
	/* For backward compatibility it is assumed that ptt_delay=0
	   means "cwdaemon shouldn't turn PTT on, at all". */

	if (current_ptt_delay && !(ptt_flag & PTT_ACTIVE_AUTO)) {
		dev->ptt(dev, ON);
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, info);


#if 0
		int rv = cw_queue_tone(current_ptt_delay * CWDAEMON_USECS_PER_MSEC, 0);	/* try to 'enqueue' delay */
		if (rv == CW_FAILURE) {	/* Old libcw may reject freq=0. */
			cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
				       "cw_queue_tone() failed: rv=%d errno=\"%s\", using udelay() instead",
				       rv, strerror(errno));
			/* TODO: wouldn't it be simpler to not to call
			   cw_queue_tone() and use only cwdaemon_udelay()? */
			cwdaemon_udelay(current_ptt_delay * CWDAEMON_USECS_PER_MSEC);
		}
#else
		cwdaemon_udelay(current_ptt_delay * CWDAEMON_USECS_PER_MSEC);
#endif

		ptt_flag |= PTT_ACTIVE_AUTO;
		cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag +PTT_ACTIVE_AUTO (0x%02x/%s)", ptt_flag, cwdaemon_debug_ptt_flags());
	}

	return;
}





/**
   \brief Switch PTT off

   \param dev current keying device
   \param info debug information displayed when performing the switching
*/
void cwdaemon_set_ptt_off(cwdevice * dev, const char *info)
{
	dev->ptt(dev, OFF);
	ptt_flag = 0;
	cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag = 0 (0x%02x/%s)", ptt_flag, cwdaemon_debug_ptt_flags());

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, info);

	return;
}





/**
   \brief Tune for a number of seconds

   Play a continuous sound for a given number of seconds.

   Parameter type is uint32_t, which gives us maximum of 4294967295
   seconds, i.e. ~136 years. Should be enough.

   TODO: change the argument type to size_t.

   \param seconds - time of tuning
*/
void cwdaemon_tune(uint32_t seconds)
{
	if (seconds > 0) {
		cw_flush_tone_queue();
		cwdaemon_set_ptt_on(global_cwdevice, "PTT (TUNE) on");

		/* make it similar to normal CW, allowing interrupt */
		for (uint32_t i = 0; i < seconds; i++) {
			cw_queue_tone(CWDAEMON_USECS_PER_SEC, current_morse_tone);
		}

		cw_send_character('e');	/* append minimal tone to return to normal flow */
	}

	return;
}





/**
   \brief Reset some initial parameters of cwdaemon and libcw

   TODO: split this function into:
   cwdaemon_reset_basic_params()
   cwdaemon_reset_libcw_output()
   and call these two functions separately instead of this one.
   This function that combines these two doesn't make much sense.

   @param dev current keying device
*/
void cwdaemon_reset_almost_all(cwdevice * dev)
{
	current_morse_speed  = default_morse_speed;
	current_morse_tone   = default_morse_tone;
	current_morse_volume = default_morse_volume;
	current_audio_system = default_audio_system;
	current_ptt_delay    = default_ptt_delay;
	current_weighting    = default_weighting;

	/* Right now there is no way to alter current_verbosity after
	   start of daemon, but it's easy to imagine a new network
	   request to modify verbosity. Maybe not very useful (to
	   change verbosity of debug strings displayed on remote
	   machine), but during development phase it may be useful.

	   Anyway... Right now there is no such request, but for
	   consistency I'm resetting the current_verbosity as well. */
	current_verbosity    = default_verbosity;

	cwdaemon_reset_libcw_output();

#ifdef CWDAEMON_GITHUB_ISSUE_6_FIXED
	fprintf(stderr, "With re-registration fixed\n");
	cw_register_keying_callback(cwdaemon_keyingevent, dev);
#endif

	return;
}





/**
   \brief Open audio sink using libcw

   \param audio_system - audio system to be used by libcw

   \return -1 on failure
   \return 0 otherwise
*/
bool cwdaemon_open_libcw_output(int audio_system)
{
	int rv = cw_generator_new(audio_system, NULL);
	if (audio_system == CW_AUDIO_OSS && rv == CW_FAILURE) {
		/* When reopening libcw output, previous audio system may
		   block audio device for a short period of time after the
		   output has been closed. In such a situation OSS may fail
		   to open audio device. Let's give it some time. */
		for (int i = 0; i < 5; i++) {
			cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "delaying switching to OSS, please wait few seconds.");
			sleep(4);
			rv = cw_generator_new(audio_system, NULL);
			if (rv == CW_SUCCESS) {
				break;
			}
		}
	}
	if (rv != CW_FAILURE) {
		rv = cw_generator_start();
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "starting generator with sound system \"%s\": %s", cw_get_audio_system_label(audio_system), rv ? "success" : "failure");

	} else {
		/* FIXME:
		   When cwdaemon failed to create a generator, and user
		   kills non-forked cwdaemon through Ctrl+C, there is
		   a memory protection error.

		   It seems that this error has been fixed with
		   changes in libcw, committed on 31.12.2012.
		   To be observed. */
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__, "failed to create generator with sound system \"%s\"", cw_get_audio_system_label(audio_system));
	}

	return rv == CW_FAILURE ? false : true;
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
	   Therefore we use default_* below.

	   However, the function is called after "current_" values
	   have been reset to "default_" values. So maybe we could use
	   "current_" values and somehow encapsulate the calls to
	   cw_set_*() functions? The calls are also made elsewhere.
	*/

	/* Delete old generator (if it exists). */
	cwdaemon_close_libcw_output();

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "setting sound system \"%s\"", cw_get_audio_system_label(default_audio_system));

	if (cwdaemon_open_libcw_output(default_audio_system)) {
		has_audio_output = true;
	} else {
		has_audio_output = false;
		return;
	}

	/* Remember that tone queue is bound to a generator.  When
	   cwdaemon switches on request to other sound system, it will
	   have to re-register the callback. */
	cw_register_tone_queue_low_callback(cwdaemon_tone_queue_low_callback, NULL, tq_low_watermark);

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

   \return false on failure
   \return true on success
*/
bool cwdaemon_get_long(const char *buf, long *lvp)
{
	errno = 0;

	char *ep;
	long lv = strtol(buf, &ep, 10);
	if (buf[0] == '\0' || *ep != '\0') {
		return false;
	}

	if (errno == ERANGE && (lv == LONG_MAX || lv == LONG_MIN)) {
		return false;
	}
	*lvp = lv;

	return true;
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

   \param cwdaemon cwdaemon instance
   \param reply - buffer for reply (allocated by caller)
   \param request - buffer with request
   \param n - size of data in request
*/
void cwdaemon_prepare_reply(cwdaemon_t * cwdaemon, char *reply, const char *request, size_t n)
{
	/* Since we need to prepare a reply, we need to mark our
	   intent to send echo. The echo (reply) will be sent to client
	   when libcw's tone queue becomes empty.

	   It is important to set this flag at the beginning of the function. */
	ptt_flag |= PTT_ACTIVE_ECHO;
	cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag +PTT_ACTIVE_ECHO (0x%02x/%s)", ptt_flag, cwdaemon_debug_ptt_flags());

	/* We are sending reply to the same host that sent a request. */
	memcpy(&cwdaemon->reply_addr, &cwdaemon->request_addr, sizeof(cwdaemon->reply_addr));
	cwdaemon->reply_addrlen = cwdaemon->request_addrlen;

	strncpy(reply, request, n);
	reply[n] = '\0'; /* FIXME: where is boundary checking? */

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "text of request: \"%s\", text of reply: \"%s\"", request, reply);
	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "now waiting for end of transmission before echoing back to client");

	return;
}



/**
   \brief Receive message from socket, act upon it

   Watch the socket and if there is an escape character check what it is,
   otherwise play morse.

   FIXME: duplicate return value (zero and zero).

   \return 0 when an escape code has been received
   \return 0 when no request or an empty request has been received
   \return 1 when a text request has been played
*/
int cwdaemon_receive(void)
{
	/* The request may be a printable string, so +1 for ending NUL
	   added somewhere below is necessary. */
	char request_buffer[CWDAEMON_MESSAGE_SIZE_MAX + 1]; /* TODO 2022.03.06: verify if we really need the +1. */

	ssize_t recv_rc = cwdaemon_recvfrom(&g_cwdaemon, request_buffer, CWDAEMON_MESSAGE_SIZE_MAX);

	if (recv_rc == -2) {
		/* Sender has closed connection. */
		return 0;
	} else if (recv_rc == -1) {
		/* TODO: should we really exit?
		   Shouldn't we recover from the error? */
		exit(EXIT_FAILURE);
	} else if (recv_rc == 0) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "...recv_from (no data)");
		return 0;
	} else {
		; /* pass */
	}

	request_buffer[recv_rc] = '\0';

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "-------------------");
	/* TODO: replace the magic number 27 with constant. */
	if (request_buffer[0] != 27) {
		/* No ESCAPE. All received data should be treated
		   as text to be sent using Morse code.

		   Note that this does not exclude possibility of
		   caret request (e.g. "some text^"), which does
		   require sending a reply to client. Such request is
		   correctly handled by cwdaemon_play_request(). */
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "request: \"%s\"", request_buffer);
		if ((strlen(request_buffer) + strlen(request_queue)) <= CWDAEMON_REQUEST_QUEUE_SIZE_MAX - 1) {
			strcat(request_queue, request_buffer);
			cwdaemon_play_request(request_queue);
		} else {
			; /* TODO: how to handle this case? */
		}
		return 1;
	} else {
		/* Don't print literal escape value, use <ESC>
		   symbol. First reason is that the literal value
		   doesn't look good in console (some non-printable
		   glyph), second reason is that printing <ESC>c to
		   terminal makes funny things with the lines already
		   printed to the terminal (tested in xfce4-terminal
		   and xterm). */
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "escaped request: \"<ESC>%s\"", request_buffer + 1);
		cwdaemon_handle_escaped_request(request_buffer);
		return 0;
	}
}




/**
   The function may call exit() if a request from client asks the
   daemon to exit.
*/
void cwdaemon_handle_escaped_request(char *request)
{
	cwdevice * dev = global_cwdevice;
	long lv;

	/* Take action depending on Escape code. */
	switch ((int) request[1]) {
	case '0':
		/* Reset all values. */
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested resetting of parameters");
		request_queue[0] = '\0';
		cwdaemon_reset_almost_all(dev);
		wordmode = 0;
		async_abort = 0;
		global_cwdevice->reset(global_cwdevice);

		ptt_flag = 0;
		cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag = 0 (0x%02x/%s)", ptt_flag, cwdaemon_debug_ptt_flags());
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "resetting completed");

		break;
	case '2':
		/* Set speed of Morse code, in words per minute. */
		if (cwdaemon_params_wpm(&current_morse_speed, request + 2)) {
			cw_set_send_speed(current_morse_speed);
		}
		break;
	case '3':
		/* Set tone (frequency) of morse code, in Hz.
		   The code assumes that minimal valid frequency is zero. */
		assert (CW_FREQUENCY_MIN == 0);
		if (cwdaemon_params_tone(&current_morse_tone, request + 2)) {
			if (current_morse_tone > 0) {

				cw_set_frequency(current_morse_tone);
				cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "tone: %d Hz", current_morse_tone);

				/* TODO: Should we really be adjusting
				   volume when the command is for
				   frequency? It would be more
				   "elegant" not to do so. */
				cw_set_volume(current_morse_volume);

			} else { /* current_morse_tone==0, sidetone off */
				cw_set_volume(0);
				cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "volume off");
			}
		}
		break;
	case '4':
		/* Abort currently sent message. */
		if (wordmode) {
			cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "requested aborting of message - ignoring (word mode is active)");
		} else {
			cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "requested aborting of message - executing (character mode is active)");
			if (ptt_flag & PTT_ACTIVE_ECHO) {
				cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "echo \"break\"");
				cwdaemon_sendto(&g_cwdaemon, "break\r\n");
			}
			request_queue[0] = '\0';
			cw_flush_tone_queue();
			cw_wait_for_tone_queue();
			if (ptt_flag) {
				cwdaemon_set_ptt_off(global_cwdevice, "PTT off");
			}
			ptt_flag &= 0;
			cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag = 0 (0x%02x/%s)", ptt_flag, cwdaemon_debug_ptt_flags());
		}
		break;
	case '5':
		/* Exit cwdaemon. */
		errno = 0;
#if 0
		char address[INET_ADDRSTRLEN];
		inet_ntop(g_cwdaemon.request_addr.sin_family, (struct in_addr*) &(g_cwdaemon.request_addr.sin_addr.s_addr),
			  address, INET_ADDRSTRLEN);
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested exit of daemon (client address: %s)", address);
#else
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested exit of daemon");
#endif
		exit(EXIT_SUCCESS);

	case '6':
		/* Set uninterruptable (word mode). */
		request[0] = '\0';
		request_queue[0] = '\0';
		wordmode = 1;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "wordmode set");
		break;
	case '7':
		/* Set weighting of morse code dits and dashes.
		   Remember that cwdaemon uses values in range
		   -50/+50, but libcw accepts values in range
		   20/80. This is why you have the calculation
		   when calling cw_set_weighting(). */
		if (cwdaemon_params_weighting(&current_weighting, request + 2)) {
			cw_set_weighting(current_weighting * 0.6 + CWDAEMON_MORSE_WEIGHTING_MAX);
		}
		break;
	case '8': {
		/* Set type of keying device. */
		cwdaemon_params_cwdevice(request + 2);

		break;
	}
	case '9':
		/* Change network port number.
		   TODO: why this is obsolete? */
		cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__,
			       "obsolete request \"9\" (change network port), ignoring");
		break;
	case 'a':
		/* PTT keying on or off */
		cwdaemon_params_ptt_on_off(request + 2);

		break;
	case 'b':
		/* SSB way. */
#if defined(HAVE_LINUX_PPDEV_H) || defined(HAVE_DEV_PPBUS_PPI_H)
		if (!cwdaemon_get_long(request + 2, &lv)) {
			break;
		}

		if (lv) {
			if (global_cwdevice->ssbway) {
				global_cwdevice->ssbway(global_cwdevice, SOUNDCARD);
				cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "\"SSB way\" set to SOUNDCARD", PACKAGE);
			} else {
				cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__, "\"SSB way\" to SOUNDCARD unimplemented");
			}
		} else {
			if (global_cwdevice->ssbway) {
				global_cwdevice->ssbway(global_cwdevice, MICROPHONE);
				cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "\"SSB way\" set to MICROPHONE");
			} else {
				cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__, "\"SSB way\" to MICROPHONE unimplemented");
			}
		}
#else
		cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__, "\"SSB way\" through parallel port unavailable (parallel port not configured).");
#endif
		break;
	case 'c':
		{
			/* FIXME: change this uint32_t to size_t. */
			uint32_t seconds = 0;
			/* Tune for a number of seconds. */
			if (cwdaemon_params_tune(&seconds, request + 2)) {
				cwdaemon_tune(seconds);
			}
			break;
		}
	case 'd':
		{
			/* Set PTT delay (TOD, Turn On Delay).
			   The value is milliseconds. */

			int rv = cwdaemon_params_pttdelay(&current_ptt_delay, request + 2);

			if (rv == 0) {
				/* Value totally invalid. */
				cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
					       "invalid requested PTT delay [ms]: \"%s\" (should be integer between %d and %d inclusive)",
					       request + 2,
					       CWDAEMON_PTT_DELAY_MIN, CWDAEMON_PTT_DELAY_MAX);
			} else if (rv == 1) {
				/* Value totally valid. Information
				   debug string has been already
				   printed in
				   cwdaemon_params_pttdelay(). */
			} else { /* rv == 2 */
				/* Value invalid (out-of-range), but
				   acceptable when sent over network
				   request and then clipped to be
				   in-range. Value has been clipped in
				   cwdaemon_params_pttdelay(), but a
				   warning debug string must be
				   printed here. */
				cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__,
					       "requested PTT delay [ms] out of range: \"%s\", clipping to \"%d\" (should be between %d and %d inclusive)",
					       request + 2,
					       CWDAEMON_PTT_DELAY_MAX,
					       CWDAEMON_PTT_DELAY_MIN, CWDAEMON_PTT_DELAY_MAX);
			}

			if (rv && current_ptt_delay == 0) {
				cwdaemon_set_ptt_off(global_cwdevice, "ensure PTT off");
			}
		}

		break;
	case 'e':
		/* Set band switch output on parport bits 9 (MSB), 8, 7, 2 (LSB). */
#if defined(HAVE_LINUX_PPDEV_H) || defined(HAVE_DEV_PPBUS_PPI_H)
		if (!cwdaemon_get_long(request + 2, &lv)) {
			break;
		}

		/* We use four bits to select band, this gives 16
		   bands: 0 - 15. */
		if (lv >= 0 && lv <= 15) {
			cwdaemon_switch_band(global_cwdevice, lv);
		}
#else
		cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__, "band switching through parallel port is unavailable (parallel port not configured)");
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
		if (cwdaemon_params_system(&current_audio_system, request + 2)) {
			/* Handle valid request for changing sound system. */
			cwdaemon_close_libcw_output();

			if (cwdaemon_open_libcw_output(current_audio_system)) {
				has_audio_output = true;
			} else {
				/* Fall back to NULL audio system. */
				cwdaemon_close_libcw_output();
				if (cwdaemon_open_libcw_output(CW_AUDIO_NULL)) {

					cwdaemon_debug(CWDAEMON_VERBOSITY_W, __func__, __LINE__,
						       "fall back to \"Null\" sound system");
					current_audio_system = CW_AUDIO_NULL;
					has_audio_output = true;
				} else {
					cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
						       "failed to fall back to \"Null\" sound system");
					has_audio_output = false;
				}
			}

			if (has_audio_output) {

				/* Tone queue is bound to a
				   generator. Creating new generator
				   requires re-registering the
				   callback. */
				cw_register_tone_queue_low_callback(cwdaemon_tone_queue_low_callback, NULL, tq_low_watermark);

				/* This call recalibrates length of
				   dot and dash. */
				cw_set_frequency(current_morse_tone);

				cw_set_send_speed(current_morse_speed);
				cw_set_volume(current_morse_volume);

				/* Regardless if we are using
				   "default" or "current" parameters,
				   the gap is always zero. */
				cw_set_gap(0);

				cw_set_weighting(current_weighting * 0.6 + CWDAEMON_MORSE_WEIGHTING_MAX);
			}
		}
		break;
	}
	case 'g':
		/* Set volume of sound, in percents. */
		if (cwdaemon_params_volume(&current_morse_volume, request + 2)) {
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

		cwdaemon_prepare_reply(&g_cwdaemon, reply_buffer, request + 1, strlen(request + 1));
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "reply is ready, waiting for message from client (reply: \"%s\")", reply_buffer);
		/* cwdaemon will wait for queue-empty callback before
		   sending the reply. */
		break;
	} /* switch ((int) request[1]) */

	return;
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
	//cw_block_callback(true);

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
			/* '^' can be found at the end of request, and
			   it means "echo text of current request back
			   to client once you finish playing it". */
			cwdaemon_prepare_reply(&g_cwdaemon, reply_buffer, request, strlen(request));

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

			cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "Morse character \"%c\" to be queued in libcw", *x);
			cw_send_character(*x);
			cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "Morse character \"%c\" has been queued in libcw", *x);

			x++;
			if (cw_get_gap() == 2) {
				if (*x == '^') {
					/* '^' is supposed to be the
					   last character in the
					   message, meaning that all
					   that was before it should
					   be used as reply text. So
					   x++ will jump to ending
					   NUL. */
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

	//cw_block_callback(false);

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

   \param arg current keying device
   \param keystate state of a key, as seen by libcw
*/
void cwdaemon_keyingevent(void * arg, int keystate)
{
	cwdevice * dev = (cwdevice *) arg;
	if (keystate == 1) {
		dev->cw(dev, ON);
	} else {
		dev->cw(dev, OFF);
	}

	inactivity_seconds = 0;

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "keying event \"%d\"", keystate);

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
	int len = cw_get_tone_queue_length();
	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "low TQ callback: start, TQ len = %d, PTT flag = 0x%02x/%s",
		       len, ptt_flag, cwdaemon_debug_ptt_flags());

	if (len > tq_low_watermark) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "low TQ callback: TQ len larger than watermark, TQ len = %d", len);
	}

	if (ptt_flag == PTT_ACTIVE_AUTO
	    /* PTT is (most probably?) on, in purely automatic mode.
	       This means that as soon as there are no new chars to
	       play, we should turn PTT off. */

	    && request_queue[0] == '\0'
	    /* No new text has been queued in the meantime. */

	    && cw_get_tone_queue_length() <= tq_low_watermark) {
		/* TODO: check if this third condition is really necessary. */
		/* Originally it was 'cw_get_tone_queue_length() <= 1',
		   I'm guessing that '1' here was the same '1' as the
		   third argument to cw_register_tone_queue_low_callback().
		   Feel free to correct me ;) */


		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "low TQ callback: branch 1, PTT flag = 0x%02x/%s", ptt_flag, cwdaemon_debug_ptt_flags());

		cwdaemon_set_ptt_off(global_cwdevice, "PTT (auto) off");

	} else if (ptt_flag & PTT_ACTIVE_ECHO) {
		/* PTT_ACTIVE_ECHO: client has used special request to
		   indicate that it is waiting for reply (echo) from
		   the server (i.e. cwdaemon) after the server plays
		   all characters. */

		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "low TQ callback: branch 2, PTT flag = 0x%02x/%s", ptt_flag, cwdaemon_debug_ptt_flags());

		/* Since echo is being sent, we can turn the flag off.
		   For some reason cwdaemon works better when we turn the
		   flag off before sending the reply, rather than turning
		   if after sending the reply. */
		ptt_flag &= ~PTT_ACTIVE_ECHO;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "low TQ callback: PTT flag -PTT_ACTIVE_ECHO, PTT flag = 0x%02x/%s", ptt_flag, cwdaemon_debug_ptt_flags());


		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "low TQ callback: echoing \"%s\" back to client             <----------", reply_buffer);
		strcat(reply_buffer, "\r\n"); /* Ensure exactly one CRLF */
		cwdaemon_sendto(&g_cwdaemon, reply_buffer);
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
			cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "low TQ callback: queueing two empty tones");
			cw_queue_tone(1, 0); /* ensure Q-empty condition again */
			cw_queue_tone(1, 0); /* when trailing gap also 'sent' */
		}
	} else {
		/* TODO: how to correctly handle this case?
		   Should we do something? */
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "low TQ callback: branch 3, PTT flag = 0x%02x/%s", ptt_flag, cwdaemon_debug_ptt_flags());
	}

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "low TQ callback: end, TQ len = %d, PTT flag = 0x%02x/%s",
		       cw_get_tone_queue_length(), ptt_flag, cwdaemon_debug_ptt_flags());

	return;

}



static const char   *cwdaemon_args_short = "d:hniy:I:f:o:p:P:s:t:T:v:Vw:x:";

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
	{ "verbosity",   required_argument,       0, 0},  /* Verbosity of cwdaemon's debug strings. */
	{ "libcwflags",  required_argument,       0, 0},  /* libcw's debug flags. */
	{ "debugfile",   required_argument,       0, 0},  /* Path to output debug file. */
	{ "system",      required_argument,       0, 0},  /* Audio system. */
	{ "options",     required_argument,       0, 0},  /* Driver-specific options. */
	{ "help",        no_argument,             0, 0},  /* Print help text and exit. */

	{ 0,             0,                       0, 0} };




void cwdaemon_args_process_long(int argc, char *argv[])
{
	int option_index = 0;

	int c = 0;

	while ((c = getopt_long(argc, argv, cwdaemon_args_short, cwdaemon_args_long, &option_index)) != -1) {
		if (c == 0) {

			cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__,
				       "long option \"%s\"%s%s\n",
				       cwdaemon_args_long[option_index].name,
				       optarg ? "=" : "",
				       optarg ? optarg : "");

			const char *optname = cwdaemon_args_long[option_index].name;

			if (!strcmp(optname, "cwdevice")) {
				if (!cwdaemon_params_cwdevice(optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "nofork")) {
				cwdaemon_params_nofork();

			} else if (!strcmp(optname, "port")) {
				if (!cwdaemon_params_network_port(optarg, &g_network_port)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "priority")) {
				if (!cwdaemon_params_priority(&process_priority, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "wpm")) {
				if (!cwdaemon_params_wpm(&default_morse_speed, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "pttdelay")) {
				if (cwdaemon_params_pttdelay(&default_ptt_delay, optarg) != 1) {
					/* When processing command
					   line arguments we are very
					   strict, and accept only
					   fully valid optarg. */
					cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
						       "invalid requested PTT delay [ms]: \"%s\" (should be integer between %d and %d inclusive)",
						       optarg,
						       CWDAEMON_PTT_DELAY_MIN, CWDAEMON_PTT_DELAY_MAX);
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "volume")) {
				if (!cwdaemon_params_volume(&default_morse_volume, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "version")) {
				cwdaemon_params_version();
				exit(EXIT_SUCCESS);

			} else if (!strcmp(optname, "weighting")) {
				if (!cwdaemon_params_weighting(&default_weighting, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "tone")) {
				if (!cwdaemon_params_tone(&default_morse_tone, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "verbosity")) {
				if (!cwdaemon_params_set_verbosity(&default_verbosity, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "libcwflags")) {
				if (!cwdaemon_params_libcwflags(optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "debugfile")) {
				if (!cwdaemon_params_debugfile(optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "system")) {
				if (!cwdaemon_params_system(&default_audio_system, optarg)) {
					exit(EXIT_FAILURE);
				}

			} else if (!strcmp(optname, "options")) {
				if (!cwdaemon_params_options(global_cwdevice, optarg)) {
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
		if (!cwdaemon_params_cwdevice(optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'n':
		cwdaemon_params_nofork();
		break;
	case 'p':
		if (!cwdaemon_params_network_port(optarg, &g_network_port)) {
			exit(EXIT_FAILURE);
		}
		break;
#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
	case 'P':
		if (!cwdaemon_params_priority(&process_priority, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
#endif
	case 's':
		if (!cwdaemon_params_wpm(&default_morse_speed, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 't':
		if (cwdaemon_params_pttdelay(&default_ptt_delay, optarg) != 1) {
			/* When processing command line arguments we
			   are very strict, and accept only fully
			   valid optarg. */
			cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
				       "invalid requested PTT delay [ms]: \"%s\" (should be integer between %d and %d inclusive)",
				       optarg,
				       CWDAEMON_PTT_DELAY_MIN, CWDAEMON_PTT_DELAY_MAX);
			exit(EXIT_FAILURE);
		}
		break;
	case 'v':
		if (!cwdaemon_params_volume(&default_morse_volume, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'V':
		cwdaemon_params_version();
		exit(EXIT_SUCCESS);
	case 'w':
		if (!cwdaemon_params_weighting(&default_weighting, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'T':
		if (!cwdaemon_params_tone(&default_morse_tone, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'i':
		cwdaemon_params_inc_verbosity(&default_verbosity);
		break;
	case 'y':
		if (!cwdaemon_params_set_verbosity(&default_verbosity, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'I':
		if (!cwdaemon_params_libcwflags(optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'f':
		if (!cwdaemon_params_debugfile(optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'x':
		if (!cwdaemon_params_system(&default_audio_system, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	case 'o':
		if (!cwdaemon_params_options(global_cwdevice, optarg)) {
			exit(EXIT_FAILURE);
		}
		break;
	}

	return;
}


bool cwdaemon_params_cwdevice(const char *optarg)
{
	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
		       "requested cwdevice \"%s\"", optarg);

	if (!cwdaemon_cwdevice_set(&global_cwdevice, optarg)) {
		return false;
	} else {
		return true;
	}
}


void cwdaemon_params_nofork(void)
{
	if (forking) {
		printf("%s: Not forking...\n", PACKAGE);
		forking = 0;
	}
	return;
}


bool cwdaemon_params_network_port(const char *optarg, int * port)
{
	long lv = 0;
	if (!cwdaemon_get_long(optarg, &lv) || lv < 1024 || lv > 65536) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid requested port number: \"%s\"", optarg);
		return false;
	} else {
		*port = lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested port number: \"%ld\"", *port);
		return true;
	}
}


bool cwdaemon_params_priority(int *priority, const char *optarg)
{
	long lv = 0;
	if (!cwdaemon_get_long(optarg, &lv) || lv < -20 || lv > 20) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid requested process priority: \"%s\" (should be integer between -20 and 20 inclusive)",
			       optarg);
		return false;
	} else {
		*priority = (int) lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested process priority: \"%ld\"", *priority);
		return true;
	}
}


bool cwdaemon_params_wpm(int *wpm, const char *optarg)
{
	long lv = 0;
	if (!cwdaemon_get_long(optarg, &lv) || lv < CW_SPEED_MIN || lv > CW_SPEED_MAX) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__ ,
			       "invalid requested morse speed [wpm]: \"%s\" (should be integer between %d and %d inclusive)",
			       optarg, CW_SPEED_MIN, CW_SPEED_MAX);
		return false;
	} else {
		*wpm = (int) lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested morse speed [wpm]: \"%d\"", *wpm);
		return true;
	}
}


bool cwdaemon_params_tune(uint32_t *seconds, const char *optarg)
{
	long lv = 0;

	/* TODO: replace cwdaemon_get_long() with cwdaemon_get_uint32() */
	if (!cwdaemon_get_long(optarg, &lv) || lv < 0 || lv > CWDAEMON_TUNE_SECONDS_MAX) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__ ,
			       "invalid requested tuning time [s]: \"%s\" (should be integer between %d and %d inclusive)",
			       optarg, 0, CWDAEMON_TUNE_SECONDS_MAX);
		return false;
	} else {
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested tuning time [s]: \"%ld\"", lv);

		*seconds = (uint32_t) lv;

		return true;
	}
}


/**
   \brief Handle parameter specifying PTT Turn On Delay

   This function, and handling its return values by callers, isn't as
   straightforward as it could be. This is because:
   \li of some backwards-compatibility reasons,
   \li because expected behaviour when handling command line argument,
   and when handling network request, are a bit different,
   \li because negative value of \p optarg is handled differently than
   too large value of \p optarg.

   The function clearly rejects negative value passed in \p
   optarg. Return value is then 0 (a "false" value in boolean
   expressions).

   It is more tolerant when it comes to non-negative values, returning
   1 or 2 (a "true" value in boolean expressions).

   When the non-negative value is out of range (larger than limit
   accepted by cwdaemon), the value is clipped to the limit, and put
   into \p delay. Return value is then 2. Caller of the function may
   then decide to accept or reject the value.

   When the non-negative value is in range, the value is put into \p
   delay, and return value is 1. Caller of the function must accept
   the value.

   Value passed in \p optarg is copied to \p delay only when function
   returns 1 or 2.

   \return 1 if value of \optarg is acceptable when it was provided as request and as command line argument (i.e. non-negative value in range);
   \return 2 if value of \optarg is acceptable when it was provided as request, but not acceptable when it was provided as command line argument (i.e. non-negative value out of range);
   \return 0 if value of \optarg is not acceptable, regardless how it was provided (i.e. a negative value or invalid value);
*/
int cwdaemon_params_pttdelay(int *delay, const char *optarg)
{
	long lv = 0;
	if (!cwdaemon_get_long(optarg, &lv)) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid requested PTT delay [ms]: \"%s\" (should be integer between %d and %d inclusive)",
			       optarg,
			       CWDAEMON_PTT_DELAY_MIN, CWDAEMON_PTT_DELAY_MAX);

		/* 0 means "Value not acceptable in any context." */
		return 0;
	}

	if (lv > CWDAEMON_PTT_DELAY_MAX) {

		/* In theory we should reject invalid value, but for
		   some reason in some contexts we aren't very strict
		   about it. So be it. Just don't allow the value to
		   be larger than *_MAX limit. */
		*delay = CWDAEMON_PTT_DELAY_MAX;

		/* 2 means "Value in general invalid (non-negative,
		   but out of range), but in some contexts we may be
		   tolerant and accept it after it has been decreased
		   to an in-range value (*_MAX). */
		return 2;


	} else if (lv < CWDAEMON_PTT_DELAY_MIN) {

		/* In first branch of the if() we have accepted too
		   large value from misinformed client, but we can't
		   accept values that are clearly invalid (negative). */

		/* 0 means "Value is not acceptable in any context". */
		return 0;

	} else { /* Non-negative, in range. */
		*delay = (int) lv;

		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested PTT delay [ms]: \"%ld\"", *delay);

		/* 1 means "Value valid in all contexts." */
		return 1;
	}
}


bool cwdaemon_params_volume(int *volume, const char *optarg)
{
	long lv = 0;
	if (!cwdaemon_get_long(optarg, &lv) || lv < CW_VOLUME_MIN || lv > CW_VOLUME_MAX) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid requested volume [%%]: \"%s\" (should be integer between %d and %d inclusive)",
			       optarg, CW_VOLUME_MIN, CW_VOLUME_MAX);
		return false;
	} else {
		*volume = (int) lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested volume [%%]: \"%d\"", *volume);
		return true;
	}
}


void cwdaemon_params_version(void)
{
	printf("%s version %s\n", PACKAGE, VERSION);
	return;
}


bool cwdaemon_params_weighting(int *weighting, const char *optarg)
{
	long lv = 0;
	if (!cwdaemon_get_long(optarg, &lv) || lv < CWDAEMON_MORSE_WEIGHTING_MIN || lv > CWDAEMON_MORSE_WEIGHTING_MAX) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid requested weighting: \"%s\" (should be integer between %d and %d inclusive)",
			       optarg, CWDAEMON_MORSE_WEIGHTING_MIN, CWDAEMON_MORSE_WEIGHTING_MAX);
		return false;
	} else {
		*weighting = (int) lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested weighting: \"%ld\"", *weighting);
		return true;
	}
}


bool cwdaemon_params_tone(int *tone, const char *optarg)
{
	long lv = 0;
	if (!cwdaemon_get_long(optarg, &lv) || lv < CW_FREQUENCY_MIN || lv > CW_FREQUENCY_MAX) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid requested tone [Hz]: \"%s\" (should be integer between %d and %d inclusive)",
			       optarg, CW_FREQUENCY_MIN, CW_FREQUENCY_MAX);
		return false;
	} else {
		*tone = (int) lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested tone [Hz]: \"%ld\"", *tone);
		return true;
	}
}


void cwdaemon_params_inc_verbosity(int *verbosity)
{
	if (*verbosity < CWDAEMON_VERBOSITY_D) {
		(*verbosity)++;

		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested verbosity level threshold: \"%s\"", cwdaemon_verbosity_labels[*verbosity]);
	}

	return;
}


bool cwdaemon_params_set_verbosity(int *verbosity, const char *optarg)
{
	if (!strcmp(optarg, "n") || !strcmp(optarg, "N")) {
		*verbosity = CWDAEMON_VERBOSITY_N;
	} else if (!strcmp(optarg, "e") || !strcmp(optarg, "E")) {
		*verbosity = CWDAEMON_VERBOSITY_E;
	} else if (!strcmp(optarg, "w") || !strcmp(optarg, "W")) {
		*verbosity = CWDAEMON_VERBOSITY_W;
	} else if (!strcmp(optarg, "i") || !strcmp(optarg, "I")) {
		*verbosity = CWDAEMON_VERBOSITY_I;
	} else if (!strcmp(optarg, "d") || !strcmp(optarg, "D")) {
		*verbosity = CWDAEMON_VERBOSITY_D;
	} else {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid requested verbosity level: \"%s\"", optarg);
		return false;
	}

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
		       "requested verbosity level threshold: \"%s\"", cwdaemon_verbosity_labels[*verbosity]);
	return true;
}


bool cwdaemon_params_libcwflags(const char *optarg)
{
	long lv = 0;
	if (!cwdaemon_get_long(optarg, &lv)) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid requested debug flags: \"%s\" (should be numeric value)", optarg);
		libcw_debug_flags = 0;

		return false;
	} else {
		libcw_debug_flags = lv;
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested libcw debug flags: \"%ld\"", libcw_debug_flags);
		return true;
	}
}


bool cwdaemon_params_debugfile(const char *optarg)
{
	if (!strcmp(optarg, "syslog")) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "debug output file \"%s\" not implemented yet, try it in later versions of %s",
			       optarg, PACKAGE);
		return false;
	}

	cwdaemon_debug_f_path = strdup(optarg);
	if (!cwdaemon_debug_f_path) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "failed to copy path to debug file \"%s\"", optarg);
		return false;
	} else {
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested debug file path: \"%s\"", cwdaemon_debug_f_path);
		return true;
	}
}


bool cwdaemon_params_system(int *system, const char *optarg)
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
		/* TODO: print only those audio systems that are
		   supported on given machine. */
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid requested sound system: \"%s\" (use c(onsole), o(ss), a(lsa), p(ulseaudio), n(ull - no audio), or s(oundcard - autoselect from OSS/ALSA/PulseAudio))",
			       optarg);
		return false;
	}

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
		       "requested sound system: \"%s\" (\"%s\")", optarg, cw_get_audio_system_label(*system));
	return true;
}


bool cwdaemon_params_ptt_on_off(const char *optarg)
{
	long lv = 0;

	/* PTT keying on or off */
	if (!cwdaemon_get_long(optarg, &lv)) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid requested PTT state: \"%s\" (should be numeric value \"0\" or \"1\")", optarg);
		return false;
	} else {
		cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
			       "requested PTT state: \"%s\"", optarg);
	}

	if (lv) {
		//global_cwdevice->ptt(global_cwdevice, ON);
		if (current_ptt_delay) {
			cwdaemon_set_ptt_on(global_cwdevice, "PTT (manual, delay) on");
		} else {
			cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "PTT (manual, immediate) on");
		}

		ptt_flag |= PTT_ACTIVE_MANUAL;
		cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag +PTT_ACTIVE_MANUAL (0x%02x/%s)", ptt_flag, cwdaemon_debug_ptt_flags());

	} else if (ptt_flag & PTT_ACTIVE_MANUAL) {	/* only if manually activated */

		ptt_flag &= ~PTT_ACTIVE_MANUAL;
		cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag -PTT_ACTIVE_MANUAL (0x%02x/%s)", ptt_flag, cwdaemon_debug_ptt_flags());

		if (!(ptt_flag & !PTT_ACTIVE_AUTO)) {	/* no PTT modifiers */

			if (request_queue[0] == '\0'/* no new text in the meantime */
			    && cw_get_tone_queue_length() <= 1) {

				cwdaemon_set_ptt_off(global_cwdevice, "PTT (manual, immediate) off");
			} else {
				/* still sending, cannot yet switch PTT off */
				ptt_flag |= PTT_ACTIVE_AUTO;	/* ensure auto-PTT active */
				cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "PTT flag +PTT_ACTIVE_AUTO (0x%02x/%s)", ptt_flag, cwdaemon_debug_ptt_flags());

				cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__, "reverting from PTT (manual) to PTT (auto) now");
			}
		}
	}

	return true;
}

/**
   @brief Parse -o/--options command line argument

   @param[in/out] dev device for which to configure the options
   @param[in] optarg value of the argument

   @return true on successful parse
   @return false otherwise
*/
bool cwdaemon_params_options(cwdevice * dev, const char *optarg)
{
	/* FIXME: the program sets global_cwdevice to null device (in
	   cwdaemon_cwdevice_init()) before command line args are parsed, so
	   this pointer should never be NULL. How to recognize if -o options
	   were passed AFTER -d? */
	if (dev == NULL) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__, "-o option must be used after -d <device>");
		return false;
	}
	if (dev->optparse == NULL) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__, "selected device does not support -o option");
		return false;
	}
	return dev->optparse(dev, optarg);
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

	if (global_cwdevice && global_cwdevice->optvalidate) {
		if (!global_cwdevice->optvalidate(global_cwdevice)) {
			cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__, "cw device options are not valid");
			exit(EXIT_FAILURE);
		}
	}
	return;
}





/**
   \brief Configure file descriptor for debug output
 */
void cwdaemon_debug_open(void)
{
	/* First handle a clash of command line arguments. */
	if (forking) {
		if (cwdaemon_debug_f_path
		    && (!strcmp(cwdaemon_debug_f_path, "stdout")
			|| !strcmp(cwdaemon_debug_f_path, "stderr"))) {

			/* If the path is not specified (path is
			   NULL), it may imply that the debug output
			   should be stdout. But since we explicitly
			   asked for forking, the implied debug output
			   to stdout will be ignored. This is a valid
			   situation: explicit request for forking
			   overrides implicit expectation of debug
			   file being stdout.

			   On the other hand, if we explicitly asked
			   for debug output to be stdout or stderr,
			   and at the same time explicitly asked for
			   forking, that is completely different
			   matter. This is an error (clash of command
			   line arguments). */

			fprintf(stdout, "%s:EE: specified debug output to \"%s\" when forking\n",
				PACKAGE, cwdaemon_debug_f_path);

			exit(EXIT_FAILURE);
		}
	}


	/* Handle valid situations (after eliminating possible invalid
	   situations above). */

	if (!cwdaemon_debug_f_path) {

		if (!forking) {
			/* stdout is (for historical reasons) a
			   default file for printing debug strings
			   when not forking, so if a debug file hasn't
			   been defined in command line, the
			   cwdaemon_debug_f_path will be NULL. Treat
			   this situation accordingly.*/

			fprintf(stderr, "debug output: stdout\n");
			/* This has been already done in main(), but
			   do it again to be super-explicit. */
			cwdaemon_debug_f = stdout;
		} else {
			/* Remember that in main() the file is set to
			   stdout. Obviously we aren't going to use
			   the stdout when daemon has forked. */
			cwdaemon_debug_f = NULL;
		}

	} else if (!strcmp(cwdaemon_debug_f_path, "stdout")) {

		/* We shouldn't get here when forking (invalid
		   situation handled by first "if" in the
		   function). */
		assert(!forking);

		fprintf(stderr, "debug output: stdout\n");
		cwdaemon_debug_f = stdout;

	} else if (!strcmp(cwdaemon_debug_f_path, "stderr")) {

		/* We shouldn't get here when forking (invalid
		   situation handled by first "if" in the
		   function). */
		assert(!forking);

		fprintf(stderr, "debug output: stderr\n");
		cwdaemon_debug_f = stderr;

	} else { /* cwdaemon_debug_f_path is a path to disc file. */

		/* We can get here when both forking and not forking. */

		fprintf(stderr, "debug output: %s\n", cwdaemon_debug_f_path);

		cwdaemon_debug_f = fopen(cwdaemon_debug_f_path, "w+");
		if (!cwdaemon_debug_f) {
			fprintf(stderr, "%s: failed to open output debug file \"%s\"\n", PACKAGE, cwdaemon_debug_f_path);
			exit(EXIT_FAILURE);
		}
	}

	return;
}





/* main program: fork, open network connection and go into an endless loop
   waiting for something to happen on the UDP port */
int main(int argc, char *argv[])
{
	//cw_debug_set_flags(&cw_debug_object_dev, CW_DEBUG_GENERATOR | CW_DEBUG_SOUND_SYSTEM);
	//(&cw_debug_object_dev)->level = CW_DEBUG_DEBUG;

	/* Until a call to cwdaemon_debug_open() is made and debug
	   output is configured according to command line switches,
	   use the default "stdout" file. */
	cwdaemon_debug_f = stdout;

	atexit(cwdaemon_cwdevices_free);
	if (!cwdaemon_cwdevices_init()) {
		exit(EXIT_FAILURE);
	}

	atexit(cwdaemon_cwdevice_free);
	/* Sets global_cwdevice to null device. This may be overridden
	   with command line argument. */
	cwdaemon_cwdevice_init();

	cwdaemon_args_parse(argc, argv);
	cwdevice * dev = global_cwdevice;

	atexit(cwdaemon_debug_close);
	/* Call cwdaemon_debug_open() after parsing command line
	   arguments. Errors discovered during parsing of command line
	   args will then still be printed to stderr. Options for
	   debug output can be also passed as command line args, so
	   they weren't available until now.

	   TODO: perhaps opening debug output should be moved to a
	   later stage, so that as many debug strings as possible are
	   being printed to stdout before main daemon loop? */
	cwdaemon_debug_open();

	if (forking) {

		pid_t pid = fork();
		if (pid < 0) {
			printf("%s: Fork failed: \"%s\"\n", PACKAGE,
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

	atexit(cwdaemon_close_socket_wrapper);
	if (!cwdaemon_initialize_socket(&g_cwdaemon, g_network_port)) {
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

	/* Initialize libcw (and other things) here, this late, to be
	   sure that libcw has been initialized and is used only by
	   child process, not by parent process. */
	atexit(cwdaemon_close_libcw_output);
	cwdaemon_reset_almost_all(dev);
	if (!has_audio_output) {
		/* Failed to open libcw output. */
		exit(EXIT_FAILURE);
	}

	if (libcw_debug_flags) { /* debugging libcw as well */
		cw_set_debug_flags(libcw_debug_flags);
	}


#ifndef CWDAEMON_GITHUB_ISSUE_6_FIXED
	fprintf(stderr, "With re-registration not fixed\n");
	cw_register_keying_callback(cwdaemon_keyingevent, dev);
#endif


	/* The main loop of cwdaemon. */
	request_queue[0] = '\0';
	do {
		fd_set readfd;
		struct timeval udptime;

		FD_ZERO(&readfd);
		FD_SET(g_cwdaemon.socket_descriptor, &readfd);

		if (inactivity_seconds < 30) {
			udptime.tv_sec = 1;
			inactivity_seconds++;
		} else {
			udptime.tv_sec = 86400;
		}

		udptime.tv_usec = 0;
		/* udptime.tv_usec = 999000; */	/* 1s is more than enough */
		int fd_count = select(g_cwdaemon.socket_descriptor + 1, &readfd, NULL, NULL, &udptime);
		/* int fd_count = select(g_cwdaemon.socket_descriptor + 1, &readfd, NULL, NULL, NULL); */
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

	exit(EXIT_SUCCESS);
}





/**
   \brief Set up initial values of cw keying device names

   Allocate strings with device names, assign them to appropriate
   cwdevice_xyz.desc values.

   The strings can be later deallocated with
   cwdaemon_cwdevices_free().

   \return true
*/
bool cwdaemon_cwdevices_init(void)
{
	/* Default device description of parallel/lpt port. */
#if defined HAVE_LINUX_PPDEV_H        /* Linux, obviously. */
	cwdevice_lp.desc = strdup("parport0");
#elif defined HAVE_DEV_PPBUS_PPI_H    /* FreeBSD (ppbus/ppi). */
	cwdevice_lp.desc = strdup("ppi0");
#else                                 /* FIXME: where is OpenBSD? */

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

	return true;
}





/**
   \brief Deallocate strings with cw keying device names

   Function frees memory previously allocated with
   cwdaemon_cwdevices_init().
*/
void cwdaemon_cwdevices_free(void)
{
	if (cwdevice_ttys.desc) {
		free(cwdevice_ttys.desc);
		cwdevice_ttys.desc = NULL;
	}

	if (cwdevice_null.desc) {
		free(cwdevice_null.desc);
		cwdevice_null.desc = NULL;
	}

#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
	if (cwdevice_lp.desc) {
		free(cwdevice_lp.desc);
		cwdevice_lp.desc = NULL;
	}
#endif

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
   deallocate the copied desc with cwdaemon_cwdevices_free().

   The function can be called with \p device to which some other device
   has been already assigned.

   \param device - pointer to device variable - guessed device will be

   \return false if \p desc describes invalid device, or if memory allocation error happens
   \return true otherwise
*/
bool cwdaemon_cwdevice_set(cwdevice **device, const char *desc)
{
	int fd;

	if (!desc || !strlen(desc)) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid device description \"%s\"", desc);
		return false;
	}

	if ((fd = dev_get_tty(desc)) != -1) {
		*device = &cwdevice_ttys;
	}
#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
	else if ((fd = dev_get_parport(desc)) != -1) {
		if (geteuid()) {
			cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
				       "you must run this program as root to use parallel port");
			return false;
		}
		*device = &cwdevice_lp;
	}
#endif
	else if ((fd = dev_get_null(desc)) != -1) {
		*device = &cwdevice_null;
	} else {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "no valid device found, setting cwdevice to null device");
		/* It's better to have null device than NULL
		   pointer. */
		*device = &cwdevice_null;
	}

	if (!*device) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "invalid keyer device: \"%s\"", desc);
		return false;
	}

	/* Replace default description of device with actual
	   description provided by caller.

	   Notice that this also works for fallback null device: when
	   no valid device has been found, cwdaemon falls back to null
	   device. The name of the *intended* device (e.g. misspelled
	   port name) is being assigned to fallback null device.
	   TODO: is it a valid behaviour? Shouldn't we call the
	   fallback device "null"? */
	if ((*device)->desc) {
		free((*device)->desc);
	}
	(*device)->desc = strdup(desc);
	if (!(*device)->desc) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__,
			       "memory allocation error");
		return false;
	}

	cwdaemon_debug(CWDAEMON_VERBOSITY_I, __func__, __LINE__,
		       "keying device used: \"%s\"", (*device)->desc);
	(*device)->init(*device, fd);

	return true;

}




/**
   \brief Assign initial value to global_cwdevice

   Assign some initial value (initial device) to global_cwdevice,
   before the device will be configured through command line options.

   If cwdaemon will be started without any device specified in command
   line, we could end up with global_cwdevice being NULL pointer.

*/
void cwdaemon_cwdevice_init(void)
{
	global_cwdevice = &cwdevice_null;

	/* cwdevice_null->desc (and now global_cwdevice->desc) has
	   been set in cwdaemon_cwdevices_init(). */

	return;
}



/**
   \brief Clean up global cwdevice

   Global cwdevice has been initialized with
   cwdaemon_cwdevice_set(). Clean it up before exiting.

   This function should be registered with atexit(). It is able to
   handle situation where global cwdevice has not been initialized
   yet.

*/
void cwdaemon_cwdevice_free(void)
{
	if (global_cwdevice
	    && global_cwdevice->free) {

		global_cwdevice->free(global_cwdevice);
	}

	return;
}



void cwdaemon_close_socket_wrapper(void)
{
	cwdaemon_close_socket(&g_cwdaemon);
	return;
}


