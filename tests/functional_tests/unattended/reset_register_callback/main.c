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




/**
   @file

   Test for proper re-registration of libcw keying callback when handling
   RESET Escape request.

   Github ticket: https://github.com/acerion/cwdaemon/issues/6 ("cwdaemon
   stops working after esc 0 (reset) is issued").

   The test verifies that a problem from the ticket doesn't occur anymore.

   See also "#define CWDAEMON_GITHUB_ISSUE_6_FIXED" in src/cwdaemon.c.

   1. Start cwdaemon in the background.
   2. In test program send PLAIN request to cwdaemon to sound a short text.
   3. Let cwdaemon sound the text and manipulate keying pin of cwdevice.
   4. In test program observe (through cwdevice observer) the keying pin, inform
      a receiver about changes to the pin.
   5. In test program let the receiver interpret changes of the pin, confirm
      that a received text is the same as requested text.
   6. In test program send RESET Escape request to cwdaemon.
   7. In test program send another PLAIN request to cwdaemon to sound another short text.
   8. In test program again receive the text.

   If a RESET Escape request "broke" the cwdaemon, the cwdaemon won't be able
   to correctly manipulate key the second request (after reset) on cwdevice,
   and receiver won't receive the second message.

   If a RESET Escape request was correctly handled in cwdaemon, and cwdaemon
   correctly re-registered a callback, then cwdaemon will be able to
   correctly manipulate keying pin after the reset, which will be observed by
   cwdevice observer and then forwarded to receiver.
*/




//#define _DEFAULT_SOURCE




#include "config.h"

//#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <time.h>

#include "tests/library/client.h"
//#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/events.h"
#include "tests/library/expectations.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/server.h"
#include "tests/library/sleep.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"
#include "tests/library/test_options.h"




typedef struct test_case_t {
	char const * description;         ///< Tester-friendly description of test case.
	test_request_t plain_request_1;   ///< First PLAIN request, to be sent to cwdaemon before RESET Escape request.
	test_request_t plain_request_2;   ///< Second PLAIN request, to be sent to cwdaemon after RESET Escape request.
	event_t expected[EVENTS_MAX];     ///< Events that we expect to happen in this test case.
} test_case_t;




/// @reviewed_on{2024.05.03}
static test_case_t g_test_cases[] = {
	{ .description = "basic test case",
	  .plain_request_1 =                               TESTS_SET_BYTES("paris"),
	  .plain_request_2 =                               TESTS_SET_BYTES("finger"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris"), },
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("finger"), }, },
	}
};

static char const * g_test_name = "reset register callback";




static int evaluate_events(events_t const * recorded_events, test_case_t const * test_case);
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_options_t * test_opts);
static int testcase_run(test_case_t const * test_case, client_t * client, morse_receiver_t * morse_receiver);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static void tests_pause_between_requests(void);




/// @reviewed_on{2024.05.03}
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

	bool failure = false;
	events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
	server_t server = { .events = &events };
	client_t client = { .events = &events };
	morse_receiver_t morse_receiver = { .events = &events };
	test_case_t const * const test_case = &g_test_cases[0];


	if (0 != test_setup(&server, &client, &morse_receiver, &test_opts)) {
		test_log_err("Test: failed at setting up of test [%s]\n", g_test_name);
		failure = true;
		goto cleanup;
	}

	if (0 != testcase_run(test_case, &client, &morse_receiver)) {
		test_log_err("Test: failed at execution of testcase [%s]\n", test_case->description);
		failure = true;
		goto cleanup;
	}

	events_sort(&events);
	if (0 != evaluate_events(&events, test_case)) {
		test_log_err("Test: evaluation of events has failed for test case [%s]\n", test_case->description);
		failure = true;
		goto cleanup;
	}

 cleanup:
	if (0 != test_teardown(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at tear-down for test [%s]\n", g_test_name);
		failure = true;
	}


	if (failure) {
		test_log_err("Test: FAIL ([%s] test)\n", g_test_name);
		test_log_newline(); /* Visual separator. */
		exit(EXIT_FAILURE);
	}
	test_log_info("Test: PASS ([%s] test)\n", g_test_name);
	test_log_newline(); /* Visual separator. */
	exit(EXIT_SUCCESS);
}




/**
   @brief Prepare resources used to execute single test case

   @reviewed_on{2024.05.03}
*/
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_options_t * test_opts)
{
	const int wpm = tests_get_test_wpm();

	const server_options_t server_opts = {
		.tone               = tests_get_test_tone(),
		.sound_system       = test_opts->sound_system,
		.cwdevice_name      = TESTS_TTY_CWDEVICE_NAME,
		.wpm                = wpm,
		.supervisor_id      = test_opts->supervisor_id,
	};
	if (0 != server_start(&server_opts, server)) {
		test_log_err("Test: failed to start cwdaemon server %s\n", "");
		return -1;
	}

	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.03.22: remove casting. */
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		return -1;
	}

	const morse_receiver_config_t morse_config = { .wpm = wpm };
	if (0 != morse_receiver_configure(&morse_config, morse_receiver)) {
		test_log_err("Test: failed to configure Morse receiver %s\n", "");
		return -1;
	}

	return 0;
}




/// @reviewed_on{2024.05.03}
static int testcase_run(test_case_t const * test_case, client_t * client, morse_receiver_t * morse_receiver)
{
	/// This sends a first PLAIN request to cwdaemon server that works in
	/// initial state, i.e. RESET Escape request was not sent yet, so
	/// cwdaemon should not be broken yet.
	{
		if (0 != morse_receiver_start(morse_receiver)) {
			test_log_err("Test: failed to start Morse receiver (%d)\n", 1);
			return -1;
		}

		client_send_request(client, &test_case->plain_request_1);

		// Receive events on cwdevice (Morse code on keying pin AND/OR ptt
		// events on ptt pin).
		morse_receiver_wait_for_stop(morse_receiver);
	}


	tests_pause_between_requests();


	/* This would break the cwdaemon before a fix to
	   https://github.com/acerion/cwdaemon/issues/6 was applied. */
	test_log_info("Test: sending RESET Escape request %s\n", "");
	client_send_esc_request(client, CWDAEMON_ESC_REQUEST_RESET, "", 0);


	tests_pause_between_requests();


	/// This sends a second PLAIN request to cwdaemon that works in "after
	/// reset" state. A fixed cwdaemon should reset itself correctly.
	{
		if (0 != morse_receiver_start(morse_receiver)) {
			test_log_err("Test: failed to start Morse receiver (%d)\n", 2);
			return -1;
		}

		client_send_request(client, &test_case->plain_request_2);

		// Receive events on cwdevice (Morse code on keying pin AND/OR ptt
		// events on ptt pin).
		morse_receiver_wait_for_stop(morse_receiver);
	}

	return 0;
}




/**
   @brief Clean up resources used to execute single test case

   @reviewed_on{2024.05.03}
*/
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver)
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
		test_log_err("Test: failed to correctly stop local test instance of cwdaemon server %s\n", "");
		failure = true;
	}

	morse_receiver_deconfigure(morse_receiver);

	/*
	  Close our socket to cwdaemon server. cwdaemon may be stopped, but let's
	  still try to close socket on our end.
	*/
	client_disconnect(client);
	client_dtor(client);

	return failure ? -1 : 0;
}




/// @reviewed_on{2024.05.03}
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




/// @brief Randomized sleep between requests
///
/// Function ignores errors of random() or sleep(). There isn't much that we
/// can or should do on error in such situations - I don't want to exit tests
/// because of that.
///
/// @reviewed_on{2024.05.03}
static void tests_pause_between_requests(void)
{
	// For some reason I would prefer to have no sleep most of the time. But
	// not always.
	bool should_sleep = true;
	cwdaemon_random_biased_towards_false(4, &should_sleep);

	if (should_sleep) {
		unsigned int sleep_duration_ms = 100;
		if (0 != cwdaemon_random_uint(0, 500, &sleep_duration_ms)) {
			test_log_warn("Test: failed to randomize first sleep duration, using %u ms\n", sleep_duration_ms);
		}
		test_log_info("Test: will randomly sleep for %u ms between requests\n", sleep_duration_ms);
		if (0 != test_millisleep_nonintr(sleep_duration_ms)) {
			test_log_warn("Test: failed to sleep for %u ms\n", sleep_duration_ms);
		}
	}
}

