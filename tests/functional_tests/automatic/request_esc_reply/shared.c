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

#include "request_size.h"

#include "tests/library/events.h"
#include "tests/library/expectations.h"
#include "tests/library/log.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/server.h"
#include "tests/library/socket.h"
#include "tests/library/string_utils.h"
#include "tests/library/time_utils.h"

#include "shared.h"




/**
   @file

   Code shared between basic tests of 'esc reply' request, and tests testing
   large 'esc reply' requests.
*/




static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_options_t * test_opts);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_run(const test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver, events_t * events);
static int evaluate_events(events_t * events, const test_case_t * test_case);




int run_test_cases(const test_case_t * test_cases, size_t n_test_cases, const test_options_t * test_opts)
{
	bool failure = false;
	events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
	server_t server = { .events = &events };
	client_t client = { .events = &events };
	morse_receiver_t morse_receiver = { .events = &events };

	if (0 != test_setup(&server, &client, &morse_receiver, test_opts)) {
		test_log_err("Test: failed at test setup %s\n", "");
		failure = true;
		goto cleanup;
	}

	if (0 != test_run(test_cases, n_test_cases, &client, &morse_receiver, &events)) {
		test_log_err("Test: failed at running test cases %s\n", "");
		failure = true;
		goto cleanup;
	}

 cleanup:
	if (0 != test_teardown(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at test tear down %s\n", "");
		failure = true;
	}

	return failure ? -1 : 0;
}




/**
   @brief Evaluate events that were recorded during execution of single test
   case

   Look at contents of @p events and check if order and types of events are
   as expected.

   The events may include
     - receiving Morse code
     - receiving reply from cwdaemon server,
     - changes of state of PTT pin,
     - exiting of local instance of cwdaemon server process,

   @return 0 if events are in proper order and of proper type
   @return -1 otherwise
*/
static int evaluate_events(events_t * events, const test_case_t * test_case)
{
	events_sort(events);
	events_print(events);
	int expectation_idx = 0; /* To recognize failing expectations more easily. */




	const bool expecting_socket_reply_event = 0 != test_case->expected_reply.n_bytes;
	const int expected_events_cnt = events_get_count(test_case->expected_events);




	expectation_idx = 1;
	if (0 != expect_count_of_events(expectation_idx, events->events_cnt, expected_events_cnt)) {
		return -1;
	}
	test_log_info("Expectation %d: count of events is correct: %d\n", expectation_idx, events->events_cnt);




	/* Expectation: events are of correct type. */
	expectation_idx = 2;
	int morse_idx = -1;
	const int morse_cnt  = events_find_by_type(events, etype_morse, &morse_idx);
	if (1 != morse_cnt) {
		test_log_err("Expectation %d: incorrect count of Morse receive events: expected 1, got %d\n", expectation_idx, morse_cnt);
		return -1;
	}
	const event_t * morse_event = &events->events[morse_idx];

	int reply_idx = -1;
	const int reply_cnt = events_find_by_type(events, etype_reply, &reply_idx);
	if (expecting_socket_reply_event) {
		if (1 != reply_cnt) {
			test_log_err("Expectation %d: incorrect count of Morse receive events: expected 1, found %d\n", expectation_idx, reply_cnt);
			return -1;
		}
	} else {
		if (0 != reply_cnt) {
			test_log_err("Expectation %d: incorrect count of Morse receive events: expected 0, found %d\n", expectation_idx, reply_cnt);
			return -1;
		}
	}
	const event_t * reply_event = &events->events[reply_idx];

	test_log_info("Expectation %d: found expected events\n", expectation_idx);




	expectation_idx = 3;
	if (expecting_socket_reply_event) {
		const bool morse_is_earlier = morse_idx < reply_idx;
		if (0 != expect_morse_and_reply_events_order(expectation_idx, morse_is_earlier)) {
			return -1;
		}
	} else {
		test_log_info("Expectation %d: skipping checking order of events since there is only one event\n", expectation_idx);
	}




	expectation_idx = 4;
	if (expecting_socket_reply_event) {
		const bool morse_is_earlier = morse_idx < reply_idx;
		if (0 != expect_morse_and_reply_events_distance(expectation_idx, morse_is_earlier, morse_event, reply_event)) {
			return -1;
		}
	} else {
		test_log_info("Expectation %d: skipping checking of the expectation because socket event is not present\n", expectation_idx);
	}




	/* While this is not THE feature that needs to be verified by this test,
	   it's good to know that we received full and correct data. */
	expectation_idx = 5;
	if (0 != expect_morse_match(expectation_idx, &morse_event->u.morse, test_case->expected_morse)) {
		return -1;
	}




	expectation_idx = 6;
	if (expecting_socket_reply_event) {
		if (0 != expect_reply_match(expectation_idx, &reply_event->u.reply, &test_case->expected_reply)) {
			test_log_err("Expectation %d: expectation not met\n", expectation_idx);
			return -1;
		}
	} else {
		test_log_info("Expectation %d: skipping checking contents of socket reply because there is no socket event\n", expectation_idx);
	}




	test_log_info("Test: evaluation of test events was successful %s\n", "");

	return 0;
}




/**
   @brief Prepare resources used to execute set of test cases
*/
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_options_t * test_opts)
{
	bool failure = false;

	const int wpm = tests_get_test_wpm();

	/* Prepare local test instance of cwdaemon server. */
	const server_options_t server_opts = {
		.tone           = tests_get_test_tone(),
		.sound_system   = test_opts->sound_system,
		.nofork         = true,
		.cwdevice_name  = TESTS_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
		.supervisor_id  = test_opts->supervisor_id,
	};
	if (0 != server_start(&server_opts, server)) {
		test_log_err("Test: tailed to start cwdaemon server %s\n", "");
		failure = true;
	}


	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.01.27: remove casting. */
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		failure = true;
	}
	client_socket_receive_enable(client);
	if (0 != client_socket_receive_start(client)) {
		test_log_err("Test: failed to start socket receiver %s\n", "");
		failure = true;
	}


	const morse_receiver_config_t morse_config = { .wpm = wpm };
	if (0 != morse_receiver_configure(&morse_config, morse_receiver)) {
		test_log_err("Test: failed to configure Morse receiver %s\n", "");
		failure = true;
	}


	return failure ? -1 : 0;
}




/**
   @brief Clean up resources used to execute set of test cases
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
		test_log_err("Test: failed to correctly stop local test instance of cwdaemon %s\n", "");
		failure = true;
	}

	morse_receiver_deconfigure(morse_receiver);

	client_socket_receive_stop(client);
	client_disconnect(client);
	client_dtor(client);

	return failure ? -1 : 0;
}




/**
   @brief Run all test cases. Evaluate results (the events) of each test case.
*/
static int test_run(const test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver, events_t * events)
{
	bool failure = false;

	for (size_t i = 0; i < n_test_cases; i++) {
		const test_case_t * test_case = &test_cases[i];

		test_log_newline(); /* Visual separator. */
		test_log_info("Test: starting test case %zu / %zu: [%s]\n", i + 1, n_test_cases, test_case->description);

		/* This is the actual test. */
		{
			if (0 != morse_receiver_start(morse_receiver)) {
				test_log_err("Test: failed to start Morse receiver %s\n", "");
				failure = true;
				break;
			}

			/*
			  First we ask cwdaemon to remember a reply that should be sent back
			  to us after a message is played.

			  Then we send the message itself.

			  Then we wait for completion of job by:
			  - Morse receiver thread that decodes a Morse code on cwdevice,
			  - socket receiver that receives the remembered reply - this is the
			    most important part of this test.
			*/

			/* Ask cwdaemon to send us this reply back after playing a message. */
			client_send_request(client, &test_case->esc_request);

			/* Send the message to be played. */
			client_send_request(client, &test_case->plain_request);

			morse_receiver_wait_for_stop(morse_receiver);

			/* FIXME: shouldn't we wait here also for receipt of socket reply? Maybe some sleep here? */
		}


		/* Validation of test run. */
		if (0 != evaluate_events(events, test_case)) {
			test_log_err("Test: evaluation of events has failed for test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			break;
		}
		/* Clear stuff before running next test case. */
		events_clear(events);

		test_log_info("Test: test case %zu / %zu has succeeded\n\n", i + 1, n_test_cases);
	}

	return failure ? -1 : 0;
}



