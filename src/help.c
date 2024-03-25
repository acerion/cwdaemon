/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2024 Kamil Ignacak <acerion@wp.pl>
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

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




#include "config.h"

#include <stdio.h>

#include <libcw.h>

#include "cwdaemon.h"
#include "help.h"




void cwdaemon_args_help(void)
{
	printf("Usage: %s [options]\n", PACKAGE);
#if !defined(HAVE_GETOPT_H)
	printf("Long options are not supported on your system.\n\n");
#endif
	printf("Available options:\n");

	printf("-h, --help\n");
	printf("        Print this help and exit.\n");
	printf("-V, --version\n");
	printf("        Print version information and exit.\n");

	printf("-d, --cwdevice <device>\n");
	printf("        Use a keying device other than the default\n");
#if defined (HAVE_LINUX_PPDEV_H)
	printf("        (e.g. ttyS0,1,2, parport0,1, etc. default: parport0)\n");
#elif defined (HAVE_DEV_PPBUS_PPI_H)
	printf("        (e.g. ttyd0,1,2, ppi0,1, etc. default: ppi0)\n");
#else
#ifdef BSD
	printf("        (e.g. ttyd0,1,2, etc. default: ttyd0)\n");
#else
	printf("        (e.g. ttyS0,1,2, etc. default: ttyS0)\n");
#endif
#endif
	printf("        You can also specify a full path to the device in /dev/ dir.\n");
	printf("        Use \"null\" for dummy device (no rig keying, no ssb keying, etc.).\n");

	printf("-o, --options <option>\n");
	printf("        Specify <option> to configure device selected by -d / -cwdevice option.\n");
	printf("        Multiple <option> values can be passed in multiple -o invocations.\n");
	printf("        These options must always follow the -d / --cwdevice option\n");
	printf("        on the command line.\n");
	printf("        Driver for serial line devices understands the following options:\n");
	printf("        key=DTR|RTS|none (without spaces, default is DTR)\n");
	printf("        ptt=RTS|DTR|none (without spaces, default is RTS)\n");

	printf("-n, --nofork\n");
	printf("        Do not fork. Print debug information to stdout.\n");

	printf("-p, --port <port>\n");
	printf("        Specify a number of UDP port to listen on.\n");
	printf("        Valid values are in range <%d - %d>, inclusive.\n", CWDAEMON_NETWORK_PORT_MIN, CWDAEMON_NETWORK_PORT_MAX);
	printf("        Default port number is %d.\n", CWDAEMON_NETWORK_PORT_DEFAULT);

#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
	printf("-P, --priority <priority>\n");
	printf("        Set program's priority (-20 - 20, default: 0).\n");
#endif
	printf("-s, --wpm <speed>\n");
	printf("        Set Morse speed [wpm] (%d - %d, default: %d).\n",
	       CW_SPEED_MIN, CW_SPEED_MAX, CWDAEMON_MORSE_SPEED_DEFAULT);
	printf("-t, --pttdelay <time>\n");
	printf("        Set PTT delay [ms] (%d - %d, default: %d).\n",
	       CWDAEMON_PTT_DELAY_MIN, CWDAEMON_PTT_DELAY_MAX, CWDAEMON_PTT_DELAY_DEFAULT);
	printf("-x, --system <sound system>\n");
	printf("        Use a specific sound system:\n");
	printf("        c = console buzzer (default)\n");
	printf("        o = OSS\n");
	printf("        a = ALSA\n");
	printf("        p = PulseAudio\n");
	printf("        n = null (no audio)\n");
	printf("        s = soundcard (autoselect from OSS/ALSA/PulseAudio)\n");
	printf("-v, --volume <volume>\n");
	printf("        Set volume for soundcard output [%%] (%d - %d, default: %d).\n",
	       CW_VOLUME_MIN, CW_VOLUME_MAX, CWDAEMON_MORSE_VOLUME_DEFAULT);
	printf("-w, --weighting <weight>\n");
	printf("        Set weighting (%d - %d, default: %d).\n",
	       CWDAEMON_MORSE_WEIGHTING_MIN, CWDAEMON_MORSE_WEIGHTING_MAX, CWDAEMON_MORSE_WEIGHTING_DEFAULT);
	printf("-T, --tone <tone>\n");
	printf("        Set initial tone [Hz] (%d - %d, default: %d).\n",
	       CW_FREQUENCY_MIN, CW_FREQUENCY_MAX, CWDAEMON_MORSE_TONE_DEFAULT);
	printf("-i\n");
	printf("        Increase verbosity of debug messages printed by cwademon.\n");
	printf("        Repeat for even more verbosity (e.g. -iii).\n");
	printf("        Alternatively you can use -y/--verbosity option.\n");
	printf("-y, --verbosity <threshold>\n");
	printf("        Set verbosity threshold for debug messages printed by cwdaemon.\n");
	printf("        Recognized values:\n");
	printf("        n = none\n");
	printf("        e = error\n");
	printf("        w = warning (default)\n");
	printf("        i = information\n");
	printf("        d = debug\n");
	printf("        Alternatively you can use -i option.\n");

	printf("-I, --libcwflags <flags>\n");
	printf("        Specify value (as decimal number) of flags passed to libcw for\n");
	printf("        purposes of debugging of the libcw library.\n");

	printf("-f, --debugfile <output>\n");
	printf("        Print debug information to <output> instead of stdout.\n");
	printf("        Value of <output> can be explicitly stated as \"stdout\"\n");
	printf("        (when not forking).\n");
	printf("        Value of <output> can be also \"stderr\" (when not forking).\n");
	printf("        Special value of <output> being \"syslog\" is reserved for\n");
	printf("        future use. For now it will be rejected as invalid.\n");
	printf("        Passing path to disc file as value of <output> works in both\n");
	printf("        situations: when forking and when not forking.\n");
	printf("\n");

	return;
}





