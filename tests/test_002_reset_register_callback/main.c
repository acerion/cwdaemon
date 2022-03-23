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




/**
   Test for proper re-registration of libcw keying callback when handling RESET
   request. https://github.com/acerion/cwdaemon/issues/6
*/




#define _DEFAULT_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>




#include "../library/key_source_serial.h"
#include "../library/misc.h"
#include "../library/socket.h"




int main(void)
{
	srand(time(NULL));

	bool failure = false;
	const int wpm = 10;
	const cwdaemon_opts_t cwdaemon_opts = {
		.tone               = "1000",
		.sound_system       = CW_AUDIO_SOUNDCARD,
		.nofork             = true,
		.cwdevice_name      = TEST_CWDEVICE_NAME,
		.wpm                = wpm,
	};
	const helpers_opts_t helpers_opts = { .wpm = cwdaemon_opts.wpm };
	const cw_key_source_params_t key_source_params = {
		.param_keying = TIOCM_DTR, /* The default tty line on which keying is being done. */
		.param_ptt    = TIOCM_RTS, /* The default tty line on which ptt is being done. */
		.source_path  = "/dev/" TEST_CWDEVICE_NAME,
	};
	cwdaemon_process_t cwdaemon = { 0 };
	if (0 != cwdaemon_start_and_connect(&cwdaemon_opts, &cwdaemon)) {
		fprintf(stderr, "[EE] Failed to start cwdaemon, exiting\n");
		failure = true;
		goto cleanup;
	}
	atexit(test_helpers_cleanup);
	if (0 != test_helpers_setup(&helpers_opts, &key_source_params)) {
		fprintf(stderr, "[EE] Failed to configure test helpers, exiting\n");
		failure = true;
		goto cleanup;
	}




	/* This sends a text request to cwdaemon that works in initial state,
	   i.e. reset command was not sent yet, so cwdaemon should not be
	   broken yet. */
	if (0 != cwdaemon_play_text_and_receive(&cwdaemon, "paris", false)) {
		fprintf(stderr, "[EE] failed to send first request, exiting\n");
		failure = true;
		goto cleanup;
	}

	/* This would break the cwdaemon before a fix to
	   https://github.com/acerion/cwdaemon/issues/6 was applied. */
	cwdaemon_socket_send_request(cwdaemon.fd, CWDAEMON_REQUEST_RESET, "");

	/* This sends a text request to cwdaemon that works in "after reset"
	   state. A fixed cwdaemon should reset itself correctly. */
	if (0 != cwdaemon_play_text_and_receive(&cwdaemon, "texas", false)) {
		fprintf(stderr, "[EE] Failed to send second request, exiting\n");
		failure = true;
		goto cleanup;
	}




 cleanup:
	/* This should stop the cwdaemon that runs in background. */
	cwdaemon_process_do_delayed_termination(&cwdaemon, 100);
	cwdaemon_process_wait_for_exit(&cwdaemon);

	/* Close socket to the cwdaemon. cwdaemon is stopped, but let's still
	   try to close socket on our end. */
	if (cwdaemon.fd >= 0) {
		cwdaemon_socket_disconnect(cwdaemon.fd);
		cwdaemon.fd = -1;
	}




	if (failure) {
		exit(EXIT_FAILURE);
	} else {
		exit(EXIT_SUCCESS);
	}
}




