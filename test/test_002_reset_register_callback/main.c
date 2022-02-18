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
#define _GNU_SOURCE /* strcasestr() */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>




#include "../library/socket.h"
#include "../library/misc.h"




static void cleanup(void);




/* global variables */
static cwdaemon_process_t g_cwdaemon_child = { 0 };




static void cleanup(void)
{
	test_helpers_teardown();

	/* This should stop the cwdaemon that runs in background. */
	cwdaemon_process_do_delayed_termination(&g_cwdaemon_child, 100);
	cwdaemon_process_wait_for_exit(&g_cwdaemon_child);

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
		.tone               = "1000",
		.sound_system       = "p",
		.nofork             = true,
		.cwdevice           = "ttyS0",
		.wpm                = wpm,
	};
	if (0 != cwdaemon_start_and_connect(&opts, &g_cwdaemon_child)) {
		fprintf(stderr, "[EE] Failed to start cwdaemon, exiting\n");
		exit(EXIT_FAILURE);
	}

	test_helpers_setup(wpm);


	/* This sends a text request to cwdaemon that works in initial state,
	   i.e. reset command was not sent yet, so cwdaemon should not be
	   broken yet. */
	if (0 != cwdaemon_play_text_and_receive(&g_cwdaemon_child, "paris")) {
		fprintf(stderr, "[EE] failed to send first request\n");
		exit(EXIT_FAILURE);
	}

	/* This would break the cwdaemon before a fix to
	   https://github.com/acerion/cwdaemon/issues/6 was applied. */
	cwdaemon_socket_send_request(g_cwdaemon_child.fd, CWDAEMON_REQUEST_RESET, "");

	/* This sends a text request to cwdaemon that works in "after reset"
	   state. A fixed cwdaemon should reset itself correctly. */
	if (0 != cwdaemon_play_text_and_receive(&g_cwdaemon_child, "texas")) {
		fprintf(stderr, "[EE] Failed to send second request\n");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}




