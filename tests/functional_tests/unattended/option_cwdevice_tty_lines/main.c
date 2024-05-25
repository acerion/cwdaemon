/*
 * This file is a part of cwdaemon project.
 *
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




/// @file
///
/// Test for "-o" command line option for cwdevice.




#define _DEFAULT_SOURCE

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <sys/types.h>
//#include <sys/wait.h>
//#include <time.h>
//#include <unistd.h>

#include "tests/library/client.h"
//#include "tests/library/cwdevice_observer.h"
#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/events.h"
#include "tests/library/expectations.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/server.h"
//#include "tests/library/sleep.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"
#include "tests/library/test_options.h"




typedef struct test_case_t {
	char const * description;                /**< Tester-friendly description of test case. */
	tty_pins_t server_tty_pins;              /**< Configuration of tty pins on cwdevice used by cwdaemon server. */
	tty_pins_t observer_tty_pins;            /**< Which tty pins on cwdevice should be treated by cwdevice as keying or ptt pins. */
	test_request_t plain_request;            /**< Text to be sent to cwdaemon server in the MESSAGE request. */
	event_t expected[EVENTS_MAX];     /**< Events that we expect to happen in this test case. */
} test_case_t;




/// TODO (acerion) 2024.05.09 add a case in which cwdaemon is not started due
/// to set of options that don't pass a validation made with
/// cwdevice->options->optvalidate. Example call with such invalid set of
/// options: "./src/cwdaemon -n -d /dev/ttyUSB0 -o key=dtr -o ptt=dtr".
///
/// @reviewed_on{2024.05.05}
static test_case_t g_test_cases[] = {
	// This is a SUCCESS case.
	//
	// Pins for cwdaemon are not configured explicitly. cwdaemon uses
	// implicit default configuration of pins.
	//
	// Pins for cwdevice observer are not configured explicitly. The observer
	// uses implicit default configuration of pins.
	{ .description = "success case, setup without tty line options passed to cwdaemon",
	  .plain_request =                                 TESTS_SET_BYTES("madrit"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("madrit")  }, },
	},

	// This is a SUCCESS case.
	//
	// Pins for cwdaemon are configured explicitly through "-o" option. The
	// explicit configuration of the pins is STANDARD, i.e. the same as
	// default one.
	//
	// Pins for cwdevice observer are not configured explicitly. The observer
	// uses implicit default configuration of pins.
	{ .description = "success case, setup with explicitly setting default tty lines options passed to cwdaemon",
	  .server_tty_pins = { .explicit = true, .pin_keying = TIOCM_DTR, .pin_ptt = TIOCM_RTS },
	  .plain_request =                                 TESTS_SET_BYTES("lisbon"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("lisbon") }, },
	},

	// This is a FAILURE case.
	//
	// Pins for cwdaemon are specified explicitly through "-o" option. The
	// explicit configuration of the pins is STANDARD, i.e. the same as
	// default one: DTR is used for keying.
	//
	// Pins for cwdevice observer are specified explicitly and the
	// configuration is NON-STANDARD: RTS pin is treated as keying pin.
	//
	// Since cwdaemon and cwdevice observer have different configuration of
	// pins, the receive process will fail.
	{ .description = "failure case, cwdaemon is keying DTR, cwdevice observer is monitoring RTS",
	  .server_tty_pins   = { .explicit = true, .pin_keying = TIOCM_DTR, .pin_ptt = TIOCM_RTS },
	  .observer_tty_pins = { .explicit = true, .pin_keying = TIOCM_RTS, .pin_ptt = TIOCM_DTR },
	  .plain_request = TESTS_SET_BYTES("paris"),
	  .expected      = { { 0 } },
	},

	// This is a SUCCESS case.
	//
	// Pins for cwdaemon are specified explicitly through "-o" option. The
	// explicit configuration of the pins is NON-STANDARD: RTS is used for
	// keying.
	//
	// Pins for cwdevice observer are specified explicitly and the
	// configuration is NON-STANDARD: RTS pin is treated as keying pin.
	//
	// Since cwdaemon and cwdevice observer have the same configuration of
	// pins, the receive process will succeed.
	{ .description = "success case, cwdaemon is keying RTS, cwdevice observer is monitoring RTS",
	  .server_tty_pins   = { .explicit = true, .pin_keying = TIOCM_RTS, .pin_ptt = TIOCM_DTR },
	  .observer_tty_pins = { .explicit = true, .pin_keying = TIOCM_RTS, .pin_ptt = TIOCM_DTR },
	  .plain_request =                                 TESTS_SET_BYTES("dublin"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("dublin") }, },
	},
};




static char const * g_test_name = "option - tty lines";




static int testcase_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_case_t * test_case, test_options_t * test_opts);
static int testcase_run(const test_case_t * test_case, client_t * client, morse_receiver_t * morse_receiver);
static int testcase_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int evaluate_events(events_t const * recorded_events, test_case_t const * test_case);




/// @reviewed_on{2024.04.05}
int main(int argc, char * const * argv)
{
	if (!testing_env_is_usable(testing_env_libcw_without_signals
	                           | testing_env_real_cwdevice_is_present)) {
		test_log_err("Test: preconditions for testing env are not met, exiting test [%s]\n", g_test_name);
		exit(EXIT_FAILURE);
	}

	test_options_t test_opts = { .sound_system = CW_AUDIO_SOUNDCARD };
	if (0 != test_options_get(argc, argv, &test_opts)) {
		test_log_err("Test: failed to process env variables and command line options for test [%s]\n", g_test_name);
		exit(EXIT_FAILURE);
	}
	if (test_opts.invoked_help) {
		/* Help text was printed as requested. Now exit. */
		exit(EXIT_SUCCESS);
	}

	const uint32_t seed = cwdaemon_srandom(test_opts.random_seed);
	test_log_info("Test: random seed: 0x%08x (%u)\n", seed, seed);

	bool overall_test_failure = false;
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	for (size_t i = 0; i < n_test_cases; i++) {
		const test_case_t * test_case = &g_test_cases[i];

		test_log_newline(); /* Visual separator. */
		test_log_info("Test: starting test case %zu / %zu: [%s]\n", i + 1, n_test_cases, test_case->description);

		bool failure = false;
		events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
		server_t server = { .events = &events };
		client_t client = { .events = &events };
		morse_receiver_t morse_receiver = { .events = &events };

		if (0 != testcase_setup(&server, &client, &morse_receiver, test_case, &test_opts)) {
			test_log_err("Test: failed at setting up of test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

		if (0 != testcase_run(test_case, &client, &morse_receiver)) {
			test_log_err("Test: failed at execution of test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

		events_sort(&events);
		if (0 != evaluate_events(&events, test_case)) {
			test_log_err("Test: evaluation of events has failed for test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

	cleanup:
		if (0 != testcase_teardown(&server, &client, &morse_receiver)) {
			test_log_err("Test: failed at tear-down for test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
		}

		if (failure) {
			test_log_err("Test: test case #%zu/%zu failed, terminating\n", i + 1, n_test_cases);
			overall_test_failure = true;
			break;
		}
		test_log_info("Test: test case #%zu/%zu succeeded\n\n", i + 1, n_test_cases);
	}

	if (overall_test_failure) {
		test_log_newline(); /* Visual separator. */
		test_log_err("Test: FAIL ([%s] test)\n", g_test_name);
		exit(EXIT_FAILURE);
	}
	test_log_newline(); /* Visual separator. */
	test_log_info("Test: PASS ([%s] test)\n", g_test_name);
	exit(EXIT_SUCCESS);
}




/// @brief Prepare resources used to execute single test case
///
/// @reviewed_on{2024.04.28}
static int testcase_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_case_t * test_case, test_options_t * test_opts)
{
	const int wpm = tests_get_test_wpm();

	/* Prepare local test instance of cwdaemon server. */
	const server_options_t server_opts = {
		.tone           = tests_get_test_tone(),
		.sound_system   = test_opts->sound_system,
		.cwdevice_name  = TESTS_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
		.tty_pins       = test_case->server_tty_pins, /* Server should toggle cwdevice pins according to this config. */
		.supervisor_id  = test_opts->supervisor_id,
	};
	if (0 != server_start(&server_opts, server)) {
		test_log_err("Test: failed to start cwdaemon server %s\n", "");
		return -1;
	}


	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.01.29: remove casting. */
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		return -1;
	}


	const morse_receiver_config_t morse_config = { .observer_tty_pins_config = test_case->observer_tty_pins,
	                                               .wpm = wpm };
	if (0 != morse_receiver_configure(&morse_config, morse_receiver)) {
		test_log_err("Test: failed to configure Morse receiver %s\n", "");
		return -1;
	}

	return 0;
}




/**
   @brief Run single test case. Evaluate its results (the events).

   cwdaemon server will be playing message from testcase
   (test_case->plain_request) and will be keying a specific line of tty device
   (test_case->cwdaemon_param_keying).

   The cwdevice observer will be observing a tty line that it was told to
   observe (test_case->observer_tty_pins) and will be notifying a
   Morse-receiver about keying events.

   The Morse-receiver should correctly receive the text that cwdaemon was
   playing (unless we expect to not receive a Morse code).

   @reviewed_on{2024.04.28}
*/
static int testcase_run(const test_case_t * test_case, client_t * client, morse_receiver_t * morse_receiver)
{
	if (0 != morse_receiver_start(morse_receiver)) {
		test_log_err("Test: failed to start Morse receiver (%d)\n", 1);
		return -1;
	}

	client_send_request(client, &test_case->plain_request);

	morse_receiver_wait_for_stop(morse_receiver);

	return 0;
}




/// @brief Clean up resources used to execute single test case
///
/// @reviewed_on{2024.04.28}
static int testcase_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver)
{
	bool failure = false;

	/* Terminate local test instance of cwdaemon server. Always do it first
	   since the server is the main trigger of events in the test. */
	if (0 != local_server_stop(server, client)) {
		/*
		  Stopping a server is not a main part of a test, but if a
		  server can't be closed then it means that the main part of the
		  code has left server in bad condition. The bad condition is an
		  indication of an error in tested functionality. Therefore set
		  failure to true.
		*/
		test_log_err("Test: failed to correctly stop local test instance of cwdaemon at end of test case %s\n", "");
		failure = true;
	}

	morse_receiver_deconfigure(morse_receiver);

	/* Close our socket to cwdaemon server. */
	client_disconnect(client);
	client_dtor(client);

	return failure ? -1 : 0;
}




/**
   @brief Evaluate events that were recorded during execution of single test
   case

   The events may include
     - receiving Morse code
     - receiving reply from cwdaemon server,
     - changes of state of PTT pin,
     - exiting of local instance of cwdaemon server process,

   @reviewed_on{2024.05.05}
*/
static int evaluate_events(events_t const * recorded_events, test_case_t const * test_case)
{
	events_print(recorded_events); // For debug only.


	int expectation_idx = 0; // To recognize failing expectations more easily.
	event_t const * const expected = test_case->expected;
	event_t const * const recorded = recorded_events->events;


	// Expectation: correct count, types, order and contents of events.
	expectation_idx = 1;
	if (0 != expect_count_type_order_contents(expectation_idx, expected, recorded)) {
		return -1;
	}


	test_log_info("Test: evaluation of test events was successful for test case [%s]\n", test_case->description);

	return 0;
}

