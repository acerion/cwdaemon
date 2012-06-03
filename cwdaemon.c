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

#include <libcw.h>
#include "cwdaemon.h"

/* network vars */
socklen_t sin_len, reply_socklen;
int socket_descriptor;
struct sockaddr_in k_sin, reply_sin;
int port = 6789;		/* default UDP port we listen on */
char reply_data[256];

/* morse defaults */
#define CWDAEMON_DEFAULT_MORSE_SPEED           24
#define CWDAEMON_MIN_MORSE_SPEED     CW_SPEED_MIN
#define CWDAEMON_MAX_MORSE_SPEED     CW_SPEED_MAX


int morse_speed = 24;		/* speed (wpm) */
int morse_tone = 800;		/* tone (Hz) */
int morse_sound = 1;		/* speaker on */
int morse_volume = 70;		/* initial volume */
int wordmode = 0;		/* start in character mode */
int ptt_delay = 0;		/* default = 0 ms */
int console_sound = 1;		/* speaker on */
int soundcard_sound = 0;	/* soundcard off */

/* various variables */
int forking = 1; 		/* we fork by default */
int debuglevel = 0;		/* only debug when not forking */
int bandswitch;
int priority = 0;
int async_abort = 0;

/* flags for different states */
int ptt_timer_running = 0;	/* flag for PTT state */
int aborting = 0;
int sendingmorse = 0;



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



void cwdaemon_tune(int seconds);
void cwdaemon_keyingevent(void *arg, int keystate);


struct timeval now, end, left;	/* PTT timers */
struct timespec sleeptime, time_remainder; /* delay timers */

#define MAXMORSE 4000
char morsetext[MAXMORSE];

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
static void playmorsestring (char *x);

static int set_libcw_output (void);
static void close_libcw (void);

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
	va_list ap;
	char s[1024 + 1];

	if (level <= debuglevel) {
		va_start(ap, info);
		vsnprintf(s, 1024, info, ap);
		va_end(ap);
		printf("%s: %s \n", PACKAGE, s);
	}

	return;
}





/* delay in microseconds */
static void
udelay (unsigned long us)
{
	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = us * 1000;

	if (nanosleep (&sleeptime, &time_remainder) == -1)
	{
		if (errno == EINTR)
			nanosleep (&time_remainder, NULL);
		else
			errmsg ("Nanosleep");
	}
}


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
	if (ptt_delay && !(ptt_flag & PTT_ACTIVE_AUTO)) {
		cwdev->ptt(cwdev, ON);
		cwdaemon_debug(1, msg);
		int rv = cw_queue_tone(ptt_delay * 20, 0);	/* try to 'enqueue' delay */
		if (rv == CW_FAILURE) {			        /* old libcw may reject freq=0 */
			cwdaemon_debug(1, "cw_queue_tone failed: rv=%d errno=%s, using udelay instead",
				       rv, strerror(errno));
			udelay(ptt_delay);
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
			cw_queue_tone(1000000, morse_tone);
		}

		cw_send_character('e');	/* append minimal tone to return to normal flow */
	}

	return;
}





/* (re)set initial parameters of libcw */
static void
reset_libcw (void)
{
	/* just in case if an old generator exists */
	close_libcw ();

	cwdaemon_debug(3, "Setting console_sound=%d, soundcard_sound=%d", console_sound, soundcard_sound);
	set_libcw_output ();

	cw_set_frequency (morse_tone);
	cw_set_send_speed (morse_speed);
	cw_set_volume (morse_volume);
	cw_set_gap (0);
}

static void
close_libcw (void)
{
	cw_generator_stop ();
	cw_generator_delete ();
}

/* set up output of libcw */
static int
set_libcw_output (void)
{
	int rv = 0;
	if (soundcard_sound && !console_sound)
	{
		rv = cw_generator_new (CW_AUDIO_ALSA, NULL);
		if (rv != CW_FAILURE)
		{
			rv = cw_generator_start();
                }
	}
	else if (!soundcard_sound && console_sound)
	{
		rv = cw_generator_new (CW_AUDIO_CONSOLE, NULL);
		if (rv != CW_FAILURE)
		{
			rv = cw_generator_start();
                }
	}
	else
	{
		/* libcw can't do both soundcard and console,
		   and it has to have one and only one sound
		   system specified */
		errmsg ("Sound output specified incorrectly");
	        rv = CW_FAILURE;
	}
	return rv == CW_FAILURE ? -1 : 0;
}

/* properly parse a 'long' integer */
static int
get_long(const char *buf, long *lvp)
{
	char *ep;
	long lv;

	errno = 0;
	lv = strtol(buf, &ep, 10);
	if (buf[0] == '\0' || (*ep != '\0' && *ep != '\n'))
		return (-1);
	if (errno == ERANGE && (lv == LONG_MAX || lv == LONG_MIN))
		return (-1);
	*lvp = lv;
	return (0);
}

/* watch the socket and if there is an escape character check what it is,
   otherwise play morse. Return 0 with escape characters and empty messages.*/
static int
recv_code (void)
{
	char message[257], *ndev;
	ssize_t recv_rc;
	int valid_sdevice = 0, fd;
	long lv;

	recv_rc = recvfrom (socket_descriptor, message, sizeof (message) - 1,
		0, (struct sockaddr *) &k_sin, &sin_len);

	if (recv_rc == -1 && errno != EAGAIN)
	{
		errmsg ("Recvfrom");
		exit (1);
	}

	if (recv_rc > 0)
	{
		message[recv_rc] = '\0';
		if (message[0] != 27)
		{	/* no ESCAPE */
			cwdaemon_debug(1, "Message = %s", message);
			if ((strlen (message) + strlen (morsetext)) <= MAXMORSE - 1)
			{
				strcat (morsetext, message);
				playmorsestring (morsetext);
			}
			return 1;
		}
		else
		{	/* check ESCAPE characters */
			switch ((int) message[1])
			{
			case '0':	/* reset  all values */
				morsetext[0] = '\0';
				morse_speed = 24;
				morse_tone = 800;
				morse_volume = 70;
				console_sound = 1;
				soundcard_sound = 0;
				reset_libcw ();
				wordmode = 0;
				async_abort = 0;
				cwdev->reset (cwdev);
				ptt_flag = 0;
				cwdaemon_debug(1, "Reset all values");
				break;
			case '2':	/*speed */
				if (get_long(message + 2, &lv))
					break;
				if (lv >= CWDAEMON_MIN_MORSE_SPEED && lv <= CWDAEMON_MAX_MORSE_SPEED)
				{
					morse_speed = lv;
					cw_set_send_speed (morse_speed);
					cwdaemon_debug(1, "Speed: %d wpm", morse_speed);
				}
				break;
			case '3':	/* tone */
				if (get_long(message + 2, &lv))
					break;
				if (lv > 0 && lv < 4001)
				{
					morse_tone = lv;
					cw_set_frequency (morse_tone);
					cw_set_volume (70);
					morse_sound = 1;
					cwdaemon_debug(1, "Tone: %s Hz, volume 70%", message + 2);
				}
				else if (lv == 0)	/* sidetone off */
				{
					morse_tone = 0;
					cw_set_volume (0);
					morse_sound = 0;
					cwdaemon_debug(1, "Volume off");
				}
				break;
			case '4':	/* message abort */
				cwdaemon_debug(1, "Message abort");
				strcpy (morsetext, "");
					cw_flush_tone_queue ();
					cw_wait_for_tone_queue ();
				aborting = 1;
				break;
			case '5':	/* exit */
				cwdev->free (cwdev);
				errno = 0;
				errmsg ("Sender has told me to end the connection");
				exit (0);
				break;
			case '6':	/* set uninterruptable */
				message[0] = '\0';
				morsetext[0] = '\0';
				wordmode = 1;
				cwdaemon_debug(1, "Wordmode set");
				break;
			case '7':	/* set weighting */
				if (get_long(message + 2, &lv))
					break;
				if ((lv > -51) && (lv < 51))	/* only allowed range */
				{
					cw_set_weighting (lv * 0.6 + 50);
					cwdaemon_debug(1, "Weight: %ld", lv);
				}
				break;
			case '8':	/* device type */
				ndev = message + 2;
				cwdaemon_debug(1, "Device: %s", ndev);
				if ((fd = dev_is_tty(ndev)) != -1)
				{
					cwdev->free(cwdev);
					cwdev = &cwdevice_ttys;
					keydev = cwdev->desc = ndev;
					cwdev->init(cwdev, fd);
				}
#if defined(HAVE_LINUX_PPDEV_H) || defined(HAVE_DEV_PPBUS_PPI_H)
				else if ((fd = dev_is_parport(ndev)) != -1)
				{
					cwdev->free(cwdev);
					cwdev = &cwdevice_lp;
					keydev = cwdev->desc = ndev;
					cwdev->init(cwdev, fd);
				}
#endif
				else if ((fd = dev_is_null(ndev)) != -1)
				{
					cwdev->free(cwdev);
					cwdev = &cwdevice_null;
					keydev = cwdev->desc = ndev;
					cwdev->init(cwdev, fd);
				}
				else
					cwdaemon_debug(1, "Unknown device");
				break;
			case '9':	/* base port number */
				cwdaemon_debug(1, "Obsolete");
				break;
			case 'a':	/* PTT keying on or off */
				if (get_long(message + 2, &lv))
					break;
				if (lv)
				{
					cwdev->ptt (cwdev, ON);
					cwdaemon_debug(1, "PTT on");
						}
						else
						{
					cwdev->ptt (cwdev, OFF);
					cwdaemon_debug(1, "PTT off");
				}
				break;
			case 'b':	/* SSB way */
#if defined(HAVE_LINUX_PPDEV_H) || defined(HAVE_DEV_PPBUS_PPI_H)
				if (get_long(message + 2, &lv))
					break;
				if (lv)
				{
					if (cwdev->ssbway)
					{
						cwdev->ssbway (cwdev, SOUNDCARD);
						cwdaemon_debug(1, "SSB way set to SOUNDCARD", PACKAGE);
					}
					else
						cwdaemon_debug(1, "SSB way to SOUNDCARD unimplemented");
				}
				else
				{
					if (cwdev->ssbway)
					{
						cwdev->ssbway (cwdev, MICROPHONE);
						cwdaemon_debug(1, "SSB way set to MIC");
					}
					else
						cwdaemon_debug(1, "SSB way to MICROPHONE unimplemented");
				}
#else
				cwdaemon_debug(1, "Unavailable");
#endif
				break;
			case 'c':	/* Tune for a number of seconds */
				if (get_long(message + 2, &lv))
					break;
				if (lv <= 10)
					cwdaemon_tune(lv);
				break;
			case 'd':	/* set ptt delay (TOD, Turn On Delay) */
				if (get_long(message + 2, &lv))
					break;
				if (lv >= 0 && lv < 51)
					ptt_delay = lv * 1000;
				else
					ptt_delay = 50000;
				cwdaemon_debug(1, "PTT delay(TOD): %d ms", ptt_delay / 1000);
				if (ptt_delay == 0)
					cwdaemon_set_ptt_off("ensure PTT off");
				break;
			case 'e':	/* set bandswitch output on parport bits 2(lsb),7,8,9(msb) */
#if defined(HAVE_LINUX_PPDEV_H) || defined(HAVE_DEV_PPBUS_PPI_H)
				if (get_long(message + 2, &lv))
					break;
				if (lv <= 15 && lv >= 0) {
					bandswitch = lv;
					set_switch(bandswitch);
				}
#else
				cwdaemon_debug(1, "Parallel port unavailable");
#endif
				break;
			case 'f':	/* switch console/soundcard */
				if (!strncmp (message + 2, "n", 1))
				{
					console_sound = 0;
					soundcard_sound = 0;
					valid_sdevice = 1;
				}
				else if (!strncmp (message + 2, "c", 1))
				{
					console_sound = 1;
					soundcard_sound = 0;
					valid_sdevice = 1;
				}
				else if (!strncmp (message + 2, "s", 1))
				{
					console_sound = 0;
					soundcard_sound = 1;
					valid_sdevice = 1;
				}
				else if (!strncmp (message + 2, "b", 1))
				{
					console_sound = 1;
					soundcard_sound = 1;
					valid_sdevice = 1;
				}
				if (valid_sdevice == 1)
				{
					cwdaemon_debug(1, "Sound device: %s", message + 2);
					set_libcw_output ();
				}
				break;
			case 'g':	/* volume */
				if (get_long(message + 2, &lv))
					break;
				if (lv <= 100 && lv >= 0)
				{
					morse_volume = lv;
					cw_set_volume (morse_volume);
				}
				break;
			case 'h':   /* send echo to main program when CW playing is done */
				memcpy (&reply_sin, &k_sin, sizeof(reply_sin)); /* remember sender */
				reply_socklen = sin_len;
				strncpy (reply_data + 1, message, sizeof(reply_data) - 2);
				reply_data[sizeof(reply_data) - 1] = '\0';
				reply_data[0]='h';
				if (strlen (message) + 1 <= MAXMORSE - 1)
					strcat (morsetext, "^");
				break;
			}
			return 0;
		}
	}
	else
		cwdaemon_debug(2, "...recv_from (no data)");
	return 0;
}

/* check every character for speed increase or decrease, and play others */
static void
playmorsestring (char *x)
{
	int i = 0, validchar = 0;
	char c;

	sendingmorse = 1;
	while (*x)
	{
		c = *x;
		if ((c == '+') || (c == '-'))
		{		/* speed in- & decrease */
			if ((c == '+') && (morse_speed <= 58))
				morse_speed += 2;
			if ((c == '-') && (morse_speed >= 10))
				morse_speed -= 2;
			cw_set_send_speed (morse_speed);
		}
		else if (c == '~')
			cw_set_gap (2); /* 2 dots time additional for the next char */
		else if (c == '^')
		{
			cwdaemon_debug(1, "Echo '%s'", reply_data);
			if (strlen (reply_data) == 0) return;
			sendto (socket_descriptor, reply_data, strlen (reply_data), 0,
				(struct sockaddr *)&reply_sin, reply_socklen);
			strcpy (reply_data,"");
			break;
		}
		else
			{
			validchar = 1;
			if (ptt_delay)
			{
				if (ptt_timer_running == 1)
				{
					gettimeofday (&end, NULL);
					timer_add (&end, 0, ptt_delay);
				}
				else
				{
					cwdev->ptt (cwdev, ON);
					cwdaemon_debug(1, "PTT on");
					/* TOD */
					udelay (ptt_delay);
			}
			}
			if (c == '*') c = '+';
			cwdaemon_debug(1, "Morse = %c", c);
			cw_send_character (c);
			if (cw_get_gap () == 2) cw_set_gap (0);
			cw_wait_for_tone_queue();
			}
		x++;
		i++;
		if (i >= strlen (morsetext))
		{
			i = 0;
			break;
		}
		if (wordmode == 0)
			recv_code ();
	}
	morsetext[0] = '\0';

	/* start ptt off timer */
	if (ptt_delay && validchar)
	{
		gettimeofday (&end, NULL);
		timer_add (&end, 0, ptt_delay);
		ptt_timer_running = 1;
	}
	sendingmorse = 0;
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

	cwdaemon_debug(3, "keying event %d", keystate);

	return;
}





/* parse the command line and check for options, do some error checking */
static void
parsecommandline (int argc, char *argv[])
{
	long lv;
	int p;

	while ((p = getopt (argc, argv, "d:hnp:P:s:t:v:Vw:x:")) != -1)
	{
		switch (p)
		{
		case ':':
		case '?':
		case 'h':
			printf ("Usage: %s [option]...\n", PACKAGE);
			printf ("       -d <device>   ");
			printf ("Use a different device\n                     ");
#if defined (HAVE_LINUX_PPDEV_H)
			printf ("(e.g. ttyS0,1,2, parport0,1, etc. default = parport0)\n");
#elif defined (HAVE_DEV_PPBUS_PPI_H)
			printf ("(e.g. ttyd0,1,2, ppi0,1, etc. default = ppi0)\n");
#else
#ifdef BSD
			printf ("(e.g. ttyd0,1,2, etc. default = ttyd0)\n");
#else
			printf ("(e.g. ttyS0,1,2, etc. default = ttyS0)\n");
#endif
#endif
			printf ("       -h            ");
			printf ("Display this help and exit\n");
			printf ("       -n            ");
			printf ("Do not fork and print debug information to stdout\n");
			printf ("       -p <port>     ");
			printf ("Use a different UDP port number (> 1023, default = 6789)\n");
#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
			printf ("       -P <priority> ");
			printf ("Set cwdaemon priority (-20 ... 20, default = 0)\n");
#endif
			printf ("       -s <speed>    ");
			printf ("Set morse speed (%d ... %d wpm, default = %d)\n",
				CWDAEMON_MIN_MORSE_SPEED, CWDAEMON_MAX_MORSE_SPEED, CWDAEMON_DEFAULT_MORSE_SPEED);
			printf ("       -t <time>     ");
			printf ("Set PTT delay (0 ... 50 ms, default = 0)\n");
			printf ("       -v <volume>   ");
			printf ("Set volume for soundcard output\n");
			printf ("       -V            ");
			printf ("Output version information and exit\n");
			printf ("       -w <weight>   ");
			printf ("Set weighting (-50 ... 50, default = 0)\n");
			printf ("       -x <sdevice>  ");
			printf ("Use a different sound device\n		     ");
			printf ("(c = console (default), s = soundcard, b = both, n = none)\n");
			exit (0);
		case 'd':
			keydev = optarg;
			break;
		case 'n':
			if (forking)
			{
				printf ("%s: Not forking...\n", PACKAGE);
				forking = 0;
			}
			debuglevel++;		/* increase debug level */
			break;
		case 'p':
			if (get_long(optarg, &lv) || lv < 1024 || lv > 65536) {
				printf("%s: invalid port number - %s\n",
				    PACKAGE, optarg);
				exit(1);
			}
			port = lv;
			break;
#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
		case 'P':
			if (get_long(optarg, &lv) || lv < -20 || lv > 20) {
				printf("%s: bad priority %s (should be between -20 and 20 inclusive)\n",
				    PACKAGE, optarg);
				exit(1);
			}
			priority = lv;
			break;
#endif
		case 's':
			if (get_long(optarg, &lv) || lv < CWDAEMON_MIN_MORSE_SPEED || lv > CWDAEMON_MAX_MORSE_SPEED)
			{
				printf("%s: bad speed %s (should be between 4 and 60 inclusive)\n",
				    PACKAGE, optarg);
				exit(1);
			}
			morse_speed = lv;
			break;
		case 't':
			if (get_long(optarg, &lv) || lv < 0 || lv > 50) {
				printf("%s: bad PTT delay value %s (should be between 0 and 50 ms inclusive)\n",
				    PACKAGE, optarg);
				exit(1);
			}
			ptt_delay = 1000 * lv;
			break;
		case 'v':
			if (get_long(optarg, &lv) || lv < 0 || lv > 100) {
				printf("%s: bad volume %s (should be between 0 and 100 inclusive)\n",
				    PACKAGE, optarg);
				exit(1);
			}
			morse_volume = lv;
			break;
		case 'V':
			printf ("%s version %s\n", PACKAGE, VERSION);
			exit (0);
		case 'w':
			if (get_long(optarg, &lv) || lv < -50 || lv > 50) {
				printf("%s: bad weight %s (should be between -50 and 50 inclusive)\n",
				    PACKAGE, optarg);
				exit(1);
			}
			cw_set_weighting (lv);
			break;
		case 'x':
			if (!strncmp(optarg, "n", 1)) {
				console_sound = 0;
				soundcard_sound = 0;
			}
			else if (!strncmp(optarg, "c", 1))
			{
				console_sound = 1;
				soundcard_sound = 0;
			}
			else if (!strncmp(optarg, "s", 1))
			{
				console_sound = 0;
				soundcard_sound = 1;
			}
			else if (!strncmp(optarg, "b", 1))
			{
				console_sound = 1;
				soundcard_sound = 1;
			}
			else
			{
				printf ("Wrong sound device, use c(onsole), s(oundcard), b(oth), or n(one)\n");
				exit (1);
			}
			break;
		}
	}
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

	parsecommandline (argc, argv);

	if (debuglevel > 3) {		/* debugging cwlib as well */
		/* FIXME: pass correct libcw debug flags. */
		cw_set_debug_flags((1 << (debuglevel - 3)) -1);
	}


	if (geteuid () != 0)
	{
		printf ("You must run this program as root\n");
		exit (1);
	}

	if ((fd = dev_is_tty(keydev)) != -1)
		cwdev = &cwdevice_ttys;
#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
	else if ((fd = dev_is_parport(keydev)) != -1)
		cwdev = &cwdevice_lp;
#endif
	else if ((fd = dev_is_null(keydev)) != -1)
		cwdev = &cwdevice_null;

	if (cwdev == NULL)
	{
		printf("%s: bad keyer device: %s\n", PACKAGE, keydev);
		exit(1);
	}
	cwdev->desc = keydev;

	reset_libcw ();
	atexit (close_libcw);

	cw_register_keying_callback(cwdaemon_keyingevent, NULL);

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

	bzero (&k_sin, sizeof (k_sin));
	k_sin.sin_family = AF_INET;
	k_sin.sin_addr.s_addr = htonl (INADDR_ANY);
	k_sin.sin_port = htons (port);
	sin_len = sizeof (k_sin);

	socket_descriptor = socket (AF_INET, SOCK_DGRAM, 0);
	if (socket_descriptor == -1)
	{
		errmsg ("Socket open");
		exit (1);
	}

	if (bind(socket_descriptor, (struct sockaddr *) &k_sin,
		sizeof (k_sin)) == -1)
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

	morsetext[0] = '\0';
	do
	{
		fd_set readfd;
		struct timeval udptime;

		FD_ZERO (&readfd);
		FD_SET (socket_descriptor, &readfd);
		udptime.tv_sec = 0;
		udptime.tv_usec = 500;
		fd_count = select (socket_descriptor + 1, &readfd, NULL, NULL, &udptime);
		if (fd_count == -1 && errno != EINTR)
			errmsg ("Select");
		else
			recv_code ();

#if defined (HAVE_LINUX_PPDEV_H) || defined (HAVE_DEV_PPBUS_PPI_H)
		if (cwdev->footswitch)
			cwdev->ptt (cwdev, !((cwdev->footswitch(cwdev))));
#endif

		/* check for ptt off timer */
		if (1 == ptt_timer_running)
		{
			gettimeofday (&now, NULL);
			if (timer_sub (&left, &end, &now) <= 0)
			{
				cwdev->ptt (cwdev, OFF);
				cwdaemon_debug(1, "PTT off");
				ptt_timer_running = 0;
			}
		}
	} while (1);

	cwdev->free (cwdev);
	if (close (socket_descriptor) == -1)
	{
		errmsg ("Close socket");
		exit (1);
	}
	exit (0);
}
