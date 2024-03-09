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
 * GNU Library General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   Basic tests of "<ESC>h request" feature: prepare a reply to be sent by cwdaemon.
*/




#define _DEFAULT_SOURCE




#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tests/library/client.h"
#include "tests/library/events.h"
#include "tests/library/expectations.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/server.h"
#include "tests/library/socket.h"
#include "tests/library/string_utils.h"
#include "tests/library/test_env.h"
#include "tests/library/time_utils.h"

#include "basic.h"




typedef struct test_case_t {
	const char * description;                /** Human-readable description of the test case. */

	socket_send_data_t esc_request;          /**< What is being sent to cwdaemon server as "esc reply" request. */
	const socket_receive_data_t expected_socket_reply;

	socket_send_data_t plain_request;        /** Full text of message to be played by cwdaemon. */
	const char expected_morse_receive[400];     /**< What is expected to be received by Morse code receiver (without ending space). */
} test_case_t;




static test_case_t g_test_cases[] = {
	/* This is a SUCCESS case. We request cwdaemon server to send us empty
	   string in reply. */
	{ .description             = "success case, empty reply value - no terminating NUL in esc request",

	  .esc_request             = SOCKET_BUF_SET("\033h"),
	  .expected_socket_reply   = SOCKET_BUF_SET(    "h\r\n"), /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = SOCKET_BUF_SET("paris"),
	  .expected_morse_receive  =                "paris",

	},

	/* This is a SUCCESS case. We request cwdaemon server to send us empty
	   string in reply. This time we add explicit NUL to end of esc
	   request. */
	{ .description             = "success case, empty reply value - with terminating NUL in esc request",

	  .esc_request             = SOCKET_BUF_SET("\033h\0"),   /* Notice the explicit terminating NUL. It will be ignored by daemon. */
	  .expected_socket_reply   = SOCKET_BUF_SET(    "h\r\n"), /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = SOCKET_BUF_SET("paris"),
	  .expected_morse_receive  =                "paris",

	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   single-letter string in reply. */
	{ .description             = "success case, single-letter as a value of reply",

	  .esc_request             = SOCKET_BUF_SET("\033hX"),
	  .expected_socket_reply   = SOCKET_BUF_SET(    "hX\r\n"), /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = SOCKET_BUF_SET("paris"),
	  .expected_morse_receive  =                "paris",
	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   single-word string in reply. */
	{ .description             = "success case, a word as value of reply, no terminating NUL in esc request",

	  .esc_request             = SOCKET_BUF_SET("\033hreply"),
	  .expected_socket_reply   = SOCKET_BUF_SET(    "hreply\r\n"), /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = SOCKET_BUF_SET("paris"),
	  .expected_morse_receive  =                "paris",
	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   single-word string in reply. This time we add explicit NUL to end of
	   esc request. */
	{ .description             = "success case, a word as value of reply, with terminating NUL in esc request",

	  .esc_request             = SOCKET_BUF_SET("\033hreply\0"),   /* Notice the explicit terminating NUL. It will be ignored by daemon. */
	  .expected_socket_reply   = SOCKET_BUF_SET(    "hreply\r\n"), /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = SOCKET_BUF_SET("paris"),
	  .expected_morse_receive  =                "paris",
	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   full-sentence string in reply. */
	{ .description             = "success case, a sentence as a value of reply",
	  .esc_request             = SOCKET_BUF_SET("\033hThis is a reply to your 27th request."),
	  .expected_socket_reply   = SOCKET_BUF_SET(    "hThis is a reply to your 27th request.\r\n"), /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = SOCKET_BUF_SET("paris"),
	  .expected_morse_receive  =                "paris",
	},

	/* This is a SUCCESS case which just skips keying a character with value (-1).

	   Test case for testing how cwdaemon handles a bug in libcw.

	   libcw 8.0.0 from unixcw 3.6.1 crashes when enqueued character has
	   value ((char) -1) / ((unsigned char) 255). This has been fixed in
	   unixcw commit c4fff9622c4e86c798703d637be7cf7e9ab84a06.

	   Since cwdaemon has to still work with unfixed versions of library, it
	   has to skip (not enqueue) the character.

	   The problem is worked-around in cwdaemon by adding 'is_valid'
	   condition before calling cw_send_character().

	   TODO acerion 2024.02.18: this functional test doesn't display
	   information that cwdaemon which doesn't have a workaround is
	   experiencing a crash. It would be good to know in all functional tests
	   that cwdaemon has crashed - it would give more info to tester.

	   TODO acerion 2024.02.18: make sure that the description of <ESC>h
	   request contains the information that socket reply includes all
	   characters from requested string, including "invalid" characters.

	   TODO acerion 2024.02.18: make sure that similar test is added for
	   regular/plain message requests in the future.
	*/
	{ .description             = "message containing '-1' integer value",

	  .esc_request             = { .n_bytes =  8, .bytes = { 27, 'h', 'l', -1,  'z', 'a', 'r', 'd' } },                /* cwdaemon doesn't validate values of chars that are requested for socket reply. */
	  .expected_socket_reply   = { .n_bytes =  9, .bytes =     { 'h', 'l', -1,  'z', 'a', 'r', 'd', '\r', '\n' } },  /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = { .n_bytes = 10, .bytes = { 'p', 'a', 's', 's', 'e', 'n', -1, 'e', 'r', '\0' } },  /* Notice inserted -1' */
	  .expected_morse_receive  =                           { 'p', 'a', 's', 's', 'e', 'n',     'e', 'r', '\0' },  /* Morse message keyed on cwdevice must not contain the -1 char (the char should be skipped by cwdaemon). */
	},
};




static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_run(test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver, events_t * events);
static int evaluate_events(events_t * events, const test_case_t * test_case);




int basic_tests(void)
{
	bool failure = false;
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
	server_t server = { .events = &events };
	client_t client = { .events = &events };
	morse_receiver_t morse_receiver = { .events = &events };

	if (0 != test_setup(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at test setup %s\n", "");
		failure = true;
		goto cleanup;
	}

	if (test_run(g_test_cases, n_test_cases, &client, &morse_receiver, &events)) {
		test_log_err("Test: failed at running test cases %s\n", "");
		failure = true;
		goto cleanup;
	}

 cleanup:
	if (0 != test_teardown(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at test tear down %s\n", "");
		failure = true;
	}

	test_log_newline(); /* Visual separator. */

	if (failure) {
		test_log_err("Test: result of the basic test: FAIL %s\n", "");
		return -1;
	}

	test_log_info("Test: result of the basic test: PASS %s\n", "");
	return 0;
}




/**
   @brief Evaluate events that were reported by objects used during execution
   of single test case

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




	/* Expectation: there should be only two events. */
	expectation_idx++;
	if (2 != events->event_idx) {
		test_log_err("Expectation %d: unexpected count of events: %d (expected 2)\n", expectation_idx, events->event_idx);
		return -1;
	}
	test_log_info("Expectation %d: count of events is correct: %d\n", expectation_idx, events->event_idx);




	/* Expectation: events are of correct type. */
	expectation_idx++;
	int morse_idx = -1;
	const int morse_cnt  = events_find_by_type(events, event_type_morse_receive, &morse_idx);
	if (1 != morse_cnt) {
		test_log_err("Expectation %d: incorrect count of Morse receive events: expected 1, got %d\n", expectation_idx, morse_cnt);
		return -1;
	}
	const event_t * morse_event = &events->events[morse_idx];

	int socket_idx = -1;
	const int socket_cnt = events_find_by_type(events, event_type_client_socket_receive, &socket_idx);
	if (1 != socket_cnt) {
		test_log_err("Expectation %d: incorrect count of socket receive events: expected 1, got %d\n", expectation_idx, socket_cnt);
		return -1;
	}
	const event_t * socket_event = &events->events[socket_idx];

	test_log_info("Expectation %d: found expected events\n", expectation_idx);




	expectation_idx++;
	if (0 != expect_morse_and_socket_event_order(expectation_idx, morse_idx, socket_idx)) {
		return -1;
	}




	expectation_idx++;
	if (0 != expect_morse_and_socket_events_distance(expectation_idx, morse_idx, morse_event, socket_idx, socket_event)) {
		return -1;
	}




	/* While this is not THE feature that needs to be verified by this test,
	   it's good to know that we received full and correct data. */
	expectation_idx++;
	if (0 != expect_morse_receive_match(expectation_idx, morse_event->u.morse_receive.string, test_case->expected_morse_receive)) {
		return -1;
	}




	expectation_idx++;
	if (0 != expect_socket_reply_match(expectation_idx, &socket_event->u.socket_receive, &test_case->expected_socket_reply)) {
		return -1;
	}




	/* Evaluation found no errors. */
	test_log_info("Test: Evaluation of test events was successful %s\n", "");

	return 0;
}




/**
   @brief Prepare resources used to execute set of test cases
*/
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver)
{
	bool failure = false;

	const int wpm = test_get_test_wpm();

	/* Prepare local test instance of cwdaemon server. */
	cwdaemon_opts_t server_opts = {
		.tone           = test_get_test_tone(),
		.sound_system   = CW_AUDIO_SOUNDCARD,
		.nofork         = true,
		.cwdevice_name  = TEST_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
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
	if (0 != morse_receiver_ctor(&morse_config, morse_receiver)) {
		test_log_err("Test: failed to create Morse receiver %s\n", "");
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

	morse_receiver_dtor(morse_receiver);

	client_socket_receive_stop(client);
	client_disconnect(client);
	client_dtor(client);

	return failure ? -1 : 0;
}




/**
   @brief Run all test cases. Evaluate results (the events) of each test case.
*/
static int test_run(test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver, events_t * events)
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
			client_send_message(client, test_case->esc_request.bytes, test_case->esc_request.n_bytes);

			/* Send the message to be played. Notice that we use
			   test_case->esc_request.n_bytes_to_send to specify count of
			   bytes to be sent. */
			client_send_message(client, test_case->plain_request.bytes, test_case->plain_request.n_bytes);

			morse_receiver_wait(morse_receiver);

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




