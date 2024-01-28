/*
 * This file is a part of cwdaemon project.
 *
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "tests/library/client.h"
#include "tests/library/cwdevice_observer.h"
#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/events.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/process.h"
#include "tests/library/random.h"
#include "tests/library/sleep.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"




events_t g_events = { .mutex = PTHREAD_MUTEX_INITIALIZER };




typedef struct test_case_t {
	const char * description;                /**< Tester-friendly description of test case. */
	tty_pins_t server_tty_pins;              /**< Configuration of tty pins on cwdevice used by cwdaemon server. */
	const char * string_to_play;             /**< Text to be sent to cwdaemon server by cwdaemon client in a request. */
	bool expected_failed_receive;            /**< Is a failure of Morse-receiving process expected in this testcase? */
	tty_pins_t observer_tty_pins;            /**< Which tty pins on cwdevice should be treated by cwdevice as keying or ptt pins. */
} test_case_t;




static test_case_t g_test_cases[] = {
	/* This is a SUCCESS case. This is a basic case where cwdaemon is
	   executed without -o options, so it uses default tty lines.

	   Configuration of pins for cwdevice observer is implicitly default. */
	{ .description             = "success case, standard setup without tty line options passed to cwdaemon",
	  .string_to_play          = "paris",
	},

	/* This is a SUCCESS case. This is an almost-basic case where
	   cwdaemon is executed with -o options but the options still tell
	   cwdaemon to use default tty lines.

	   Configuration of pins for cwdevice observer is implicitly default. */
	{ .description             = "success case, standard setup with explicitly setting default tty lines options passed to cwdaemon",
	  .server_tty_pins         = { .explicit = true, .pin_keying = TIOCM_DTR, .pin_ptt = TIOCM_RTS },
	  .string_to_play          = "paris",
	},

	/* This is a FAIL case. cwdaemon is told to toggle a DTR while
	   keying, but a cwdevice observer (and thus a Morse receiver) is told to look at
	   RTS for keying events. */
	{ .description             = "failure case, cwdaemon is keying DTR, cwdevice observer is monitoring RTS",
	  .server_tty_pins         = { .explicit = true, .pin_keying = TIOCM_DTR, .pin_ptt = TIOCM_RTS },
	  .string_to_play          = "paris",
	  .expected_failed_receive = true,
	  .observer_tty_pins       = { .explicit = true, .pin_keying = TIOCM_RTS, .pin_ptt = TIOCM_DTR }
	},

	/* This is a SUCCESS case. cwdaemon is told to toggle a RTS while
	   keying, and a cwdevice observer (and thus a receiver) is told to look
	   also at RTS for keying events. */
	{ .description             = "success case, cwdaemon is keying RTS, cwdevice observer is monitoring RTS",
	  .server_tty_pins         = { .explicit = true, .pin_keying = TIOCM_RTS, .pin_ptt = TIOCM_DTR },
	  .string_to_play          = "paris",
	  .observer_tty_pins       = { .explicit = true, .pin_keying = TIOCM_RTS, .pin_ptt = TIOCM_DTR },
	},
};




static int evaluate_events(const events_t * events, const test_case_t * test_case);




int main(void)
{
#if 0
	if (!test_env_is_usable(test_env_libcw_without_signals)) {
		fprintf(stderr, "[EE] Preconditions for test env are not met, exiting\n");
		exit(EXIT_FAILURE);
	}
#endif

	const uint32_t seed = cwdaemon_srandom(0);
	fprintf(stderr, "[DD] Random seed: 0x%08x (%u)\n", seed, seed);

	int wpm = 10;
	/* Remember that some receive timeouts in tests were selected when the
	   wpm was hardcoded to 10 wpm. Picking values lower than 10 may lead to
	   overrunning the timeouts. */
	cwdaemon_random_uint(10, 15, (unsigned int *) &wpm);

	cwdaemon_opts_t server_opts = {
		.tone           = 660,
		.sound_system   = CW_AUDIO_SOUNDCARD,
		.nofork         = true,
		.cwdevice_name  = TEST_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
	};

	const size_t n = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	for (size_t i = 0; i < n; i++) {
		const test_case_t * test_case = &g_test_cases[i];
		fprintf(stderr, "\n[II] Starting test case #%zd: %s\n", i, test_case->description);

		bool failure = false;
		cwdaemon_server_t server = { 0 };
		client_t client = { 0 };

		const morse_receiver_config_t morse_config = { .observer_tty_pins_config = test_case->observer_tty_pins, .wpm = wpm };
		morse_receiver_t * morse_receiver = NULL;



		/* Prepare local test instance of cwdaemon server. */
		server_opts.tty_pins = test_case->server_tty_pins; /* Server should toggle cwdevice pins according to this config. */
		if (0 != server_start(&server_opts, &server)) {
			fprintf(stderr, "[EE] Failed to start cwdaemon server, terminating\n");
			failure = true;
			goto cleanup;
		}


		if (0 != client_connect_to_server(&client, server.ip_address, server.l4_port)) {
			test_log_err("Test: can't connect cwdaemon client to cwdaemon server %s\n", "");
			failure = true;
			goto cleanup;
		}


		morse_receiver = morse_receiver_ctor(&morse_config);
		if (0 != morse_receiver_start(morse_receiver)) {
			test_log_err("Failed to start Morse receiver (%d)\n", 1);
			failure = true;
			goto cleanup;
		}


		/*
		  The actual testing is done here.

		  cwdaemon server will be now playing given string and will be keying
		  a specific line on tty (test_case->cwdaemon_param_keying).

		  The cwdevice observer will be observing a tty line that it was told
		  to observe (test_case->key_source_param_keying) and will be
		  notifying a Morse-receiver about keying events.

		  The Morse-receiver should correctly receive the text that cwdaemon
		  was playing (unless 'expected_failed_receive' is set to true).
		*/
		client_send_request_va(&client, CWDAEMON_REQUEST_MESSAGE, "one %s", test_case->string_to_play);

		morse_receiver_wait(morse_receiver);

		if (0 != evaluate_events(&g_events, test_case)) {
			test_log_err("Evaluation of events has failed for test case %zu/%zu\n", i + 1, n);
			failure = true;
		} else {
			test_log_info("Evaluation of events was successful for test case %zu/%zu\n", i + 1, n);
		}



	cleanup:

		morse_receiver_dtor(&morse_receiver);

		/* Terminate local test instance of cwdaemon. */
		if (0 != local_server_stop(&server, &client)) {
			/*
			  Stopping a server is not a main part of a test, but if a
			  server can't be closed then it means that the main part of the
			  code has left server in bad condition. The bad condition is an
			  indication of an error in tested functionality. Therefore set
			  failure to true.
			*/
			test_log_err("Failed to correctly stop local test instance of cwdaemon at end of test case %zu/%zu\n", i + 1, n);
			failure = true;
		}

		/* Close our socket to cwdaemon server. */
		client_disconnect(&client);

		events_clear(&g_events);

		if (failure) {
			fprintf(stderr, "[EE] Test case #%zu/%zu failed, terminating\n", i + 1, n);
			exit(EXIT_FAILURE);
		} else {
			fprintf(stderr, "[II] Test case #%zu/%zu succeeded\n\n", i + 1, n);
		}
	}




	exit(EXIT_SUCCESS);
}




static int evaluate_events(const events_t * events, const test_case_t * test_case)
{
	/* Expectation 1: correct count of events. */
	if (test_case->expected_failed_receive) {
		if (0 != events->event_idx) {
			test_log_err("Expectation 1: unexpected count of events (expected failed receive): %d\n", events->event_idx);
			return -1;
		}
	} else {
		if (1 != events->event_idx) {
			test_log_err("Expectation 1: unexpected count of events (expected successful receive): %d\n", events->event_idx);
			return -1;
		}
	}
	test_log_err("Expectation 1: found expected count of events (expected %s receive): %d\n",
	             test_case->expected_failed_receive ? "unsuccessful" : "successful",
	             events->event_idx);




	if (0 == events->event_idx) {
		/* No more expectations to fulfill. */
		test_log_info("Evaluation of test events was successful %s\n", "");
		return 0;
	}




	/* Expectation 2: our event is a Morse receive event. */
	const event_t * event_morse = NULL;
	if (events->events[0].event_type != event_type_morse_receive) {
		test_log_err("Expectation 2: unexpected type of event: %d\n", events->events[0].event_type);
		return -1;
	}
	test_log_info("Expectation 2: found expected Morse event %s\n", "");
	event_morse = &events->events[0];




	/* Expectation 3: the Morse event contains correct received text. */
	if (!morse_receive_text_is_correct(event_morse->u.morse_receive.string, test_case->string_to_play)) {
		test_log_err("Expectation 3: unexpected received text [%s]\n", event_morse->u.morse_receive.string);
		return -1;
	}
	test_log_info("Expectation 3: found expected received text [%s]\n", event_morse->u.morse_receive.string);




	test_log_info("Evaluation of test events was successful %s\n", "");

	return 0;
}

