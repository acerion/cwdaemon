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
   Test of EXIT request (and of test code that starts a test instance of
   cwdaemon).
*/




#define _DEFAULT_SOURCE

/* For kill() on FreeBSD 13.2 */
#include <signal.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>




#include "../library/key_source.h"
#include "../library/key_source_serial.h"
#include "../library/misc.h"
#include "../library/process.h"
#include "../library/socket.h"
#include "../library/test_env.h"
#include "src/lib/random.h"
#include "src/lib/sleep.h"




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

	const int wpm = 10;
	bool failure = false;
	cwdaemon_process_t cwdaemon = { 0 };
	client_t client = { 0 };
	const cwdaemon_opts_t cwdaemon_opts = {
		.tone           = "1000",
		.sound_system   = CW_AUDIO_SOUNDCARD,
		.nofork         = true,
		.cwdevice_name  = TEST_CWDEVICE_NAME,
		.wpm            = wpm,
	};
	const helpers_opts_t helpers_opts = { .wpm = cwdaemon_opts.wpm };
	cw_key_source_params_t key_source_params = {
		.param_keying = TIOCM_DTR, /* The default tty line on which keying is being done. */
		.param_ptt    = TIOCM_RTS, /* The default tty line on which ptt is being done. */
		.source_path  = "/dev/" TEST_CWDEVICE_NAME,
	};
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




	/* First part of a test: test that a cwdaemon is really started by asking
	   cwdaemon to play a text and observing the text keyed on serial line
	   port. */
	{
		if (0 != client_send_and_receive(&client, "paris", false)) {
			fprintf(stderr, "[EE] cwdaemon is probably not running, exiting\n");
			failure = true;
			goto cleanup;
		}
	}




	/*
	  Second part of a test: test that EXIT request works.

	  Notice that the body of next block looks the same as implementation of
	  local_server_stop(). In this function we use the code explicitly
	  because we want to test EXIT request and we want to have it plainly
	  visible in the test code.
	*/
 cleanup:
	{
		/* First ask nicely for a clean exit. */
		client_send_request(&client, CWDAEMON_REQUEST_EXIT, "");

		/* Give cwdaemon some time to exit cleanly. */
		const int sleep_retv = sleep_nonintr(2);
		if (sleep_retv) {
			fprintf(stderr, "[ERROR] error during sleep in cleanup\n");
		}

		/* Now check if test instance of cwdaemon has disappeared as expected. */
		int wstatus = 0;
		if (0 == waitpid(cwdaemon.pid, &wstatus, WNOHANG)) {
			/* Process still exists, kill it. */
			fprintf(stderr, "[ERROR] Local test instance of cwdaemon process is still active despite being asked to exit, sending SIGKILL\n");
			/* The fact that we need to kill cwdaemon with a
			   signal is a bug. */
			kill(cwdaemon.pid, SIGKILL);
			fprintf(stderr, "[ERROR] Local test instance of cwdaemon was forcibly killed\n");
			failure = true;
		}
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



