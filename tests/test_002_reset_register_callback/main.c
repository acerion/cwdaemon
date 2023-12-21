/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2023 Kamil Ignacak <acerion@wp.pl>
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




#include "../library/cwdevice_observer_serial.h"
#include "../library/misc.h"
#include "../library/socket.h"
#include "../library/test_env.h"
#include "src/lib/random.h"




int main(void)
{
#if 0
	if (!test_env_is_usable(test_env_libcw_without_signals)) {
		fprintf(stderr, "[EE] Preconditions for test env are not met, exiting\n");
		exit(EXIT_FAILURE);
	}
#endif

	const uint32_t seed = cwdaemon_srandom(0);
	fprintf(stderr, "[INFO ] Random seed: %u\n", seed);

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
	const cwdevice_observer_params_t key_source_params = {
		.tty_pins_config = { .pin_keying = TIOCM_DTR,   /* The default tty line on which keying is being done. */
		                     .pin_ptt    = TIOCM_RTS }, /* The default tty line on which ptt is being done. */
		.source_path  = "/dev/" TEST_CWDEVICE_NAME,
	};
	cwdaemon_process_t cwdaemon = { 0 };
	client_t client = { 0 };
	if (0 != cwdaemon_start_and_connect(&cwdaemon_opts, &cwdaemon, &client)) {
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
	if (0 != client_send_and_receive(&client, "paris", false)) {
		fprintf(stderr, "[EE] failed to send first request, exiting\n");
		failure = true;
		goto cleanup;
	}

	/* This would break the cwdaemon before a fix to
	   https://github.com/acerion/cwdaemon/issues/6 was applied. */
	client_send_request(&client, CWDAEMON_REQUEST_RESET, "");

	/* This sends a text request to cwdaemon that works in "after reset"
	   state. A fixed cwdaemon should reset itself correctly. */
	if (0 != client_send_and_receive(&client, "texas", false)) {
		fprintf(stderr, "[EE] Failed to send second request, exiting\n");
		failure = true;
		goto cleanup;
	}




 cleanup:
	/* Terminate local test instance of cwdaemon. */
	if (0 != local_server_stop(&cwdaemon, &client)) {
		/*
		  Stopping a server is not a main part of a test, but if a
		  server can't be closed then it means that the main part of the
		  code has left server in bad condition. The bad condition is an
		  indication of an error in tested functionality. Therefore set
		  failure to true.
		*/
		fprintf(stderr, "[ERROR] Failed to correctly stop local test instance of cwdaemon.\n");
		failure = true;
	}

	/*
	  Close our socket to cwdaemon server. cwdaemon may be stopped, but let's
	  still try to close socket on our end.
	*/
	client_disconnect(&client);

	if (failure) {
		exit(EXIT_FAILURE);
	} else {
		exit(EXIT_SUCCESS);
	}
}




