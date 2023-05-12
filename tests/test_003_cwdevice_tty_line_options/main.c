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
   Test for "-o" cwdevice options.
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
#include "../library/test_env.h"




typedef struct {
	const char * description;
	unsigned int cwdaemon_param_keying;
	unsigned int cwdaemon_param_ptt;
	const char * string_to_play;
	bool expected_failed_receive;
	unsigned int key_source_param_keying;
	unsigned int key_source_param_ptt;
} datum_t;




static datum_t g_test_data[] = {
	/* This is a SUCCESS case. This is a basic case where cwdaemon is
	   executed without -o options, so it uses default tty lines. Key
	   source is configured to look at the default line(s) for keying
	   events. */
	{ .description             = "success case, standard setup without tty line options passed to cwdaemon",
	  .string_to_play          = "paris",
	  .key_source_param_keying = TIOCM_DTR,
	  .key_source_param_ptt    = TIOCM_RTS,
	},

	/* This is a SUCCESS case. This is an almost-basic case where
	   cwdaemon is executed with -o options but the options still tell
	   cwdaemon to use default tty lines. Key source is configured to
	   look at the default line(s) for keying events. */
	{ .description             = "success case, standard setup with default tty lines options passed to cwdaemon",
	  .cwdaemon_param_keying   = TIOCM_DTR,
	  .cwdaemon_param_ptt      = TIOCM_RTS,
	  .string_to_play          = "paris",
	  .key_source_param_keying = TIOCM_DTR,
	  .key_source_param_ptt    = TIOCM_RTS,
	},

	/* This is a FAIL case. cwdaemon is told to toggle a DTR while
	   keying, but a key source (and thus a receiver) is told to look at
	   RTS for keying events. */
	{ .description             = "failure case, cwdaemon keying DTR, key source monitoring RTS",
	  .cwdaemon_param_keying   = TIOCM_DTR,
	  .cwdaemon_param_ptt      = TIOCM_RTS,
	  .string_to_play          = "paris",
	  .expected_failed_receive = true,
	  .key_source_param_keying = TIOCM_RTS,
	  .key_source_param_ptt    = TIOCM_DTR,
	},

	/* This is a SUCCESS case. cwdaemon is told to toggle a RTS while
	   keying, and a key source (and thus a receiver) is told to look
	   also at RTS for keying events. */
	{ .description             = "success case, cwdaemon keying RTS, key source monitoring RTS",
	  .cwdaemon_param_keying   = TIOCM_RTS,
	  .cwdaemon_param_ptt      = TIOCM_DTR,
	  .string_to_play          = "paris",
	  .key_source_param_keying = TIOCM_RTS,
	  .key_source_param_ptt    = TIOCM_DTR,
	},
};




int main(void)
{
	if (!test_env_is_usable(test_env_libcw_without_signals)) {
		fprintf(stderr, "[EE] Preconditions for test env are not met, exiting\n");
		exit(EXIT_FAILURE);
	}

	srand(time(NULL));

	const int wpm = 10;
	cwdaemon_opts_t cwdaemon_opts = {
		.tone           = "1000",
		.sound_system   = CW_AUDIO_SOUNDCARD,
		.nofork         = true,
		.cwdevice_name  = TEST_CWDEVICE_NAME,
		.wpm            = wpm,
	};

	const size_t n = sizeof (g_test_data) / sizeof (g_test_data[0]);
	for (size_t i = 0; i < n; i++) {
		const datum_t * datum = &g_test_data[i];
		fprintf(stderr, "\n[II] Starting test #%zd: %s\n", i, datum->description);

		bool failure = false;

		cwdaemon_opts.param_keying   = datum->cwdaemon_param_keying;
		cwdaemon_opts.param_ptt      = datum->cwdaemon_param_ptt;

		const helpers_opts_t helpers_opts = { .wpm = cwdaemon_opts.wpm };
		const cw_key_source_params_t key_source_params = {
			.param_keying = datum->key_source_param_keying,
			.param_ptt    = datum->key_source_param_ptt,
			.source_path  = "/dev/" TEST_CWDEVICE_NAME,
		};
		cwdaemon_process_t cwdaemon = { 0 };
		if (0 != cwdaemon_start_and_connect(&cwdaemon_opts, &cwdaemon)) {
			fprintf(stderr, "[EE] Failed to start cwdaemon, exiting\n");
			failure = true;
			goto cleanup;
		}
		if (0 != test_helpers_setup(&helpers_opts, &key_source_params)) {
			fprintf(stderr, "[EE] Failed to configure test helpers, exiting\n");
			failure = true;
			goto cleanup;
		}




		/* cwdaemon will be now playing given string and will be
		   keying a specific line on tty
		   (datum->cwdaemon_param_keying).

		   The key source will be observing a tty line that it was
		   told to observe (datum->key_source_param_keying) and will
		   be notifying a receiver about keying events.

		   The receiver should receive the text that cwdaemon was
		   playing (unless 'expected_failed_receive' is set to
		   true). */
		if (0 != cwdaemon_play_text_and_receive(&cwdaemon, datum->string_to_play, datum->expected_failed_receive)) {
			fprintf(stderr, "[EE] Failed at test of datum #%zd\n", i);
			failure = true;
			goto cleanup;
		}




	cleanup:
		test_helpers_cleanup();
		/* Terminate this instance of cwdaemon. */
		cwdaemon_socket_send_request(cwdaemon.fd, CWDAEMON_REQUEST_EXIT, "");
		sleep(2);

		/* Close socket to test instance of cwdaemon. cwdaemon may be
		   stopped, but let's still try to close socket on our
		   end. */
		if (cwdaemon.fd >= 0) {
			cwdaemon_socket_disconnect(cwdaemon.fd);
			cwdaemon.fd = -1;
		}
		if (failure) {
			exit(EXIT_FAILURE);
		}
	}




	exit(EXIT_SUCCESS);
}



