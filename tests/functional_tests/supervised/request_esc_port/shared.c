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




#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcw.h>

#include "src/cwdaemon.h"
#include "tests/library/events.h"
#include "tests/library/expectations.h"
#include "tests/library/log.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/requests.h"
#include "tests/library/server.h"
#include "tests/library/string_utils.h"

#include "shared.h"




/// @file
///
/// Code shared between basic tests of PORT Escape request and (in the
/// future) non-basic tests of the request.




static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, test_options_t const * test_opts);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_run(test_case_t const * test_case, client_t * client, morse_receiver_t * morse_receiver, events_t * events);
static int evaluate_events(events_t const * recorded_events, test_case_t const * test_case);




// @reviewed_on{2024.07.05}
int run_test_cases(test_case_t const * test_cases, size_t n_test_cases, test_options_t const * test_opts, char const * test_name)
{
	// We know that there is only one test case. Different ports to be tested
	// are specified not in test case(s) but elsewhere.
	if (1 != n_test_cases) {
		test_log_err("Test: unexpected count of test cases: is %zu, expected 1\n", n_test_cases);
		return -1;
	}
	test_case_t const * test_case = &test_cases[0];


	bool failure = false;
	events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
	server_t server = { .events = &events };
	client_t client = { .events = &events };
	morse_receiver_t morse_receiver = { .events = &events };

	if (0 != test_setup(&server, &client, &morse_receiver, test_opts)) {
		test_log_err("Test: failed at test setup for [%s] test\n", test_name);
		failure = true;
		goto cleanup;
	}

	test_log_info("Test: initial port of cwdaemon server is %d\n", server.l4_port);

	if (0 != test_run(test_case, &client, &morse_receiver, &events)) {
		test_log_err("Test: failed at running test cases for [%s] test\n", test_name);
		failure = true;
		goto cleanup;
	}

 cleanup:
	if (0 != test_teardown(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at test tear down for [%s] test\n", test_name);
		failure = true;
	}

	if (failure) {
		test_log_err("Test: FAIL ([%s] test)\n", test_name);
		test_log_newline(); /* Visual separator. */
		return -1;
	}
	test_log_info("Test: PASS ([%s] test)\n", test_name);
	test_log_newline(); /* Visual separator. */
	return 0;
}




/// @brief Evaluate events that were recorded during single execution of a test case
///
/// Look at contents of @p events and check if order and types of events are as expected.
///
/// The events may include
///  - receiving Morse code
///  - receiving reply from cwdaemon server,
///  - changes of state of PTT pin,
///  - exiting of local instance of cwdaemon server process,
///
/// @reviewed_on{2024.07.05}
///
/// @return 0 if events are in proper order and of proper type
/// @return -1 otherwise
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


	// Expectation: recorded Morse event and reply event are close enough to
	// each other. Check distance of the two events on time axis.
	expectation_idx = 2;
	if (0 != expect_morse_and_reply_events_distance(expectation_idx, recorded)) {
		return -1;
	}


	test_log_info("Test: evaluation of test events was successful for test case [%s]\n", test_case->description);

	return 0;
}




/// @brief Prepare resources used to execute a test
///
/// @reviewed_on{2024.07.05}
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, test_options_t const * test_opts)
{
	// We want test cases to be executed rather quickly, so play the Morse
	// code rather quickly.
	const int wpm = TESTS_WPM_MAX;

	// Prepare local test instance of cwdaemon server.
	const server_options_t server_opts = {
		.tone           = tests_get_test_tone(),
		.sound_system   = test_opts->sound_system,
		.cwdevice_name  = TESTS_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
		.supervisor_id  = test_opts->supervisor_id,
		.log_threshold  = LOG_INFO,
	};
	if (0 != server_start(&server_opts, server)) {
		test_log_err("Test: failed to start cwdaemon server %s\n", "");
		return -1;
	}


	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { // TODO (acerion) 2024.05.14: remove casting.
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		return -1;
	}
	client_socket_receive_enable(client);
	if (0 != client_socket_receive_start(client)) {
		test_log_err("Test: failed to start socket receiver %s\n", "");
		return -1;
	}


	const morse_receiver_config_t morse_config = { .wpm = wpm };
	if (0 != morse_receiver_configure(&morse_config, morse_receiver)) {
		test_log_err("Test: failed to configure Morse receiver %s\n", "");
		return -1;
	}


	return 0;
}




/// @brief Clean up resources used to execute test
///
/// @reviewed_on{2024.07.05}
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
		test_log_err("Test: failed to correctly stop local test instance of cwdaemon %s\n", "");
		failure = true;
	}

	morse_receiver_deconfigure(morse_receiver);

	client_socket_receive_stop(client);
	client_disconnect(client);
	client_dtor(client);

	return failure ? -1 : 0;
}




/// @brief Run a test case in a loop
///
/// Evaluate results (the events) of each execution of given test case.
///
/// @reviewed_on{2024.07.05}
static int test_run(test_case_t const * test_case, client_t * client, morse_receiver_t * morse_receiver, events_t * events)
{
	bool failure = false;

	// I want to focus primarily on valid values of port, therefore .valid is
	// set to 70/100.
	tests_value_generation_probabilities_t const percentages =
		{ .valid        = 70,
		  .empty        = 15,
		  .invalid      = 15,
		  .random_bytes =  0, // This is not supported yet by tests library.
		};

	const size_t n_iterations = 10;
	for (size_t iter = 0; iter < n_iterations; iter++) {
		test_log_newline(); // Visual separator.
		test_log_info("Test: starting iteration %zu / %zu\n\n", iter + 1, n_iterations);

		test_request_t request = { 0 };
		if (0 != tests_requests_build_request_esc_port(&request, &percentages)) {
			return -1;
		}

		// This is needed to give tester possibility to recognize if/when to
		// expect new test case.
		test_log_info("Test: this test case will try switching port %s\n", "");
		test_log_info("Test: press any key to run the test case %s\n", "");

		// gcc 13.2.0 on FreeBSD 14.1 complains about getchar():
		// "warning: assuming signed overflow does not occur when changing X +- C1 cmp C2 to X cmp C2 -+ C1 [-Wstrict-overflow]"
		// The warning is reported for function's closing brace.
		getchar();

		// This is the actual test.
		{
			if (0 != morse_receiver_start(morse_receiver)) {
				test_log_err("Test: failed to start Morse receiver in iteration %zu\n", iter);
				return -1;
			}

			// Tell cwdaemon server to switch to a new port.
			if (0 != client_send_request(client, &request)) {
				test_log_err("Test: failed to send PORT Escape request %s\n", "");
				return -1;
			}

			// TODO (acerion) 2024.05.14: introduce random sleep between
			// PORT and REPLY Escape requests?

			// Now we ask cwdaemon to remember a reply that should be sent
			// back to us after a message is played.
			//
			// Then we send the message itself.
			//
			// Then we wait for completion of job by:
			//  - Morse receiver thread that decodes a Morse code on cwdevice - there is an explicit wait below
			//  - socket receiver that receives the remembered reply - there is an implicit wait behind the scenes.

			// Ask cwdaemon to send us this reply back after playing a
			// message.
			client_send_request(client, &test_case->reply_esc_request);

			// Send PLAIN message to be played and keyed on cwdevice and
			// played through sound system.
			client_send_request(client, &test_case->plain_request);

			// Receive events on cwdevice (Morse code on keying pin AND/OR
			// ptt events on ptt pin).
			morse_receiver_wait_for_stop(morse_receiver);

			// A reply has been received implicitly by client for which we
			// called client_socket_receive_enable()/start(). FIXME (acerion)
			// 2024.05.14: shouldn't we explicitly wait here also for receipt
			// of reply? Maybe some sleep here?
		}


		// Validation of events that were recorded during execution of test
		// case.
		events_sort(events);
		if (0 != evaluate_events(events, test_case)) {
			test_log_err("Test: evaluation of events has failed in iteration %zu / %zu for test case [%s]\n", iter + 1, n_iterations, test_case->description);
			failure = true;
			break;
		}
		// Clear stuff before running next test case.
		events_clear(events);

		test_log_info("Test: iteration %zu / %zu: test case [%s] has succeeded\n\n", iter + 1, n_iterations, test_case->description);
	}

	return failure ? -1 : 0;
}




