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




static void cleanup(void);




static cwdaemon_process_t g_cwdaemon_child = { 0 };




static void cleanup(void)
{
	test_helpers_teardown();

	/* cwdaemon is stopped, but let's still try to close socket on our
	   end. */
	cwdaemon_socket_disconnect(g_cwdaemon_child.fd);
}




int main(void)
{
	srand(time(NULL));
	int wpm = 10;
	atexit(cleanup);

	cwdaemon_opts_t opts = {
		.tone           = "1000",
		.sound_system   = "p",
		.nofork         = "-n",
		.cwdevice       = "ttyS0",
		.wpm            = wpm,
	};
	if (0 != cwdaemon_start_and_connect(&opts, &g_cwdaemon_child)) {
		fprintf(stderr, "[EE] Failed to start cwdaemon, exiting\n");
		exit(EXIT_FAILURE);
	}

	test_helpers_setup(wpm);




	/* Test that a cwdaemon is really started by asking cwdaemon to play
	   a text and observing the text keyed on serial line port. */
	{
		if (0 != cwdaemon_play_text_and_receive(&g_cwdaemon_child, "paris")) {
			fprintf(stderr, "[EE] cwdaemon is probably not running\n");
			exit(EXIT_FAILURE);
		}
	}




	/* Test that EXIT request works. */
	{
		/* First ask nicely for a clean exit. */
		cwdaemon_socket_send_request(g_cwdaemon_child.fd, CWDAEMON_REQUEST_EXIT, "");

		/* Give cwdaemon some time to exit cleanly. */
		sleep(2);

		int wstatus = 0;
		if (0 == waitpid(g_cwdaemon_child.pid, &wstatus, WNOHANG)) {
			/* Process still exists, kill it. */
			fprintf(stderr, "[EE] Child cwdaemon process is still active despite being asked to exit, sending SIGKILL\n");
			/* The fact that we need to kill cwdaemon with a signal is a
			   bug. It will be detected by a test executable when the
			   executable calls wait() on child pid. */
			kill(g_cwdaemon_child.pid, SIGKILL);
			exit(EXIT_FAILURE);
		}
	}




	exit(EXIT_SUCCESS);
}



