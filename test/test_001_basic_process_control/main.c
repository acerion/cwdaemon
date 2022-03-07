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




#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>




#include "../library/process.h"
#include "../library/socket.h"
#include "../library/misc.h"
#include "../library/key_source.h"
#include "../library/key_source_serial.h"




static void cwdaemon_cleanup(void);




static cwdaemon_process_t g_cwdaemon = { 0 };




/**
   Close socket to test instance of cwdaemon. cwdaemon may be stopped, but
   let's still try to close socket on our end.
*/
static void cwdaemon_cleanup(void)
{
	if (g_cwdaemon.fd >= 0) {
		cwdaemon_socket_disconnect(g_cwdaemon.fd);
		g_cwdaemon.fd = -1;
	}
}




int main(void)
{
	srand(time(NULL));
	int wpm = 10;

	cwdaemon_opts_t cwdaemon_opts = {
		.tone           = "1000",
		.sound_system   = "p",
		.nofork         = "-n",
		.cwdevice       = "ttyS0",
		.wpm            = wpm,
	};
	atexit(cwdaemon_cleanup);
	if (0 != cwdaemon_start_and_connect(&cwdaemon_opts, &g_cwdaemon)) {
		fprintf(stderr, "[EE] Failed to start cwdaemon, exiting\n");
		exit(EXIT_FAILURE);
	}

	atexit(test_helpers_cleanup);
	const helpers_opts_t helpers_opts = { .wpm = cwdaemon_opts.wpm };
	const cw_key_source_params_t key_source_params = {
		.param_keying = TIOCM_DTR, /* The default tty line on which keying is being done. */
		.param_ptt    = TIOCM_RTS, /* The default tty line on which ptt is being done. */
	};
	if (0 != test_helpers_setup(&helpers_opts, &key_source_params)) {
		fprintf(stderr, "[EE] Failed to configure test helpers, exiting\n");
		exit(EXIT_FAILURE);
	}



	/* Test that a cwdaemon is really started by asking cwdaemon to play
	   a text and observing the text keyed on serial line port. */
	{
		if (0 != cwdaemon_play_text_and_receive(&g_cwdaemon, "paris", false)) {
			fprintf(stderr, "[EE] cwdaemon is probably not running, exiting\n");
			exit(EXIT_FAILURE);
		}
	}




	/* Test that EXIT request works. */
	{
		/* First ask nicely for a clean exit. */
		cwdaemon_socket_send_request(g_cwdaemon.fd, CWDAEMON_REQUEST_EXIT, "");

		/* Give cwdaemon some time to exit cleanly. */
		sleep(2);

		int wstatus = 0;
		if (0 == waitpid(g_cwdaemon.pid, &wstatus, WNOHANG)) {
			/* Process still exists, kill it. */
			fprintf(stderr, "[EE] Child cwdaemon process is still active despite being asked to exit, sending SIGKILL\n");
			/* The fact that we need to kill cwdaemon with a signal is a
			   bug. It will be detected by a test executable when the
			   executable calls wait() on child pid. */
			kill(g_cwdaemon.pid, SIGKILL);
			fprintf(stderr, "[EE] cwdaemon was forcibly killed, exiting\n");
			exit(EXIT_FAILURE);
		}
	}




	exit(EXIT_SUCCESS);
}



