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
   Test cases that send to cwdaemon plain requests that have size (count of
   characters) close to cwdaemon's maximum size of requests. The requests are
   slightly smaller, equal to and slightly larger than the size of cwdaemon's
   buffer..

   cwdaemon's buffer used to receive network messages has
   CWDAEMON_MESSAGE_SIZE_MAX==256 bytes. If a plain request sent to cwdaemon
   is larger than that, it will be truncated.
*/




#include "libcw.h"
#define _DEFAULT_SOURCE




#include "config.h"

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
#include "tests/library/test_defines.h"
#include "tests/library/test_env.h"
#include "tests/library/time_utils.h"




typedef struct test_case_t {
	const char * description;             /**< Tester-friendly description of test case. */
	socket_send_data_t plain_request;     /**< Bytes to be sent to cwdaemon server in the plain MESSAGE request. */
	const char * expected_morse_receive;  /**< What is expected to be received by Morse code receiver (without ending space). */
} test_case_t;




/* Helper definition to shorten strings in test cases. Chars at position 21
   till 250, inclusive. */
#define CHARS_21_250 \
	                    "kukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku"




static test_case_t g_test_cases[] = {
	/*
	  In this case a full plain request is keyed on cwdevice and received by
	  Morse code receiver.
	*/
	{ .description = "plain request with size smaller than cwdaemon's receive buffer - 254 bytes (without NUL)",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "1234"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "1234",
	},
	/*
	  In this case a full plain request is keyed on cwdevice and received by
	  Morse code receiver.
	*/
	{ .description = "plain request with size smaller than cwdaemon's receive buffer - 254+1 bytes (with NUL)",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "1234\0"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "1234",
	},



	/*
	  In this case a full plain request is keyed on cwdevice and received by
	  Morse code receiver.
	*/
	{ .description = "plain request with size smaller than cwdaemon's receive buffer - 255 bytes (without NUL)",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "12345"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "12345",
	},
	/*
	  In this case a full plain request is keyed on cwdevice and received by
	  Morse code receiver.
	*/
	{ .description = "plain request with size equal to cwdaemon's receive buffer - 255+1 bytes (with NUL)",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "12345\0"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "12345",
	},



	/*
	  In this case a full plain request is keyed on cwdevice and received by
	  Morse code receiver.
	*/
	{ .description = "plain request with size equal to cwdaemon's receive buffer - 256 bytes (without NUL)",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "123456"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "123456",
	},
	/*
	  In this case a full plain request is keyed on cwdevice and received by
	  Morse code receiver.

	  Notice that in this case cwdaemon's receive code will drop only the
	  terminating NUL. The non-present NUL will have no impact on further
	  actions of cwdaemon or on contents of keyed Morse message.
	*/
	{ .description = "plain request with size larger than cwdaemon's receive buffer - 256+1 bytes (with NUL)",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "123456\0"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "123456",
	},



	/*
	  In this case only a truncated plain request is keyed on cwdevice and
	  received by Morse code receiver. Request's characters that won't fit
	  into cwdaemon's receive buffer will be dropped by cwdaemon's receive
	  code and won't be keyed on cwdevice.

	  This case could be described as "failure case" because Morse receiver
	  will return something else than what client has sent to cwdaemon
	  server. But we know that cwdaemon server will drop extra char(s) from
	  the plain request, and we know what cwdaemon server will key on
	  cwdevice. And this test case is expecting and testing exactly this
	  behaviour.

	  Morse receiver will receive only first 256 chars. This is what Morse
	  code receiver will receive when this test tries to play a message with
	  count of chars that is larger than cwdaemon's receive buffer (the
	  receive buffer has space for 256 chars). The last character from
	  request will be dropped by cwdaemon's receive code.

	  Count of bytes in the sent request doesn't include terminating NUL.
	*/
	{ .description = "plain request with size larger than cwdaemon's receive buffer - 257 bytes (without NUL); TRUNCATION of Morse receive",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "1234567"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "123456",
	},
	/*
	  In this case only a truncated plain request is keyed on cwdevice and
	  received by Morse code receiver. Request's characters that won't fit
	  into cwdaemon's receive buffer will be dropped by cwdaemon's receive
	  code and won't be keyed on cwdevice.

	  This case could be described as "failure case" because Morse receiver
	  will return something else than what client has sent to cwdaemon
	  server. But we know that cwdaemon server will drop extra char(s) from
	  the plain request, and we know what cwdaemon server will key on
	  cwdevice. And this test case is expecting and testing exactly this
	  behaviour.

	  Morse receiver will receive only first 256 chars. This is what Morse
	  code receiver will receive when this test tries to play a message with
	  count of chars that is larger than cwdaemon's receive buffer (the
	  receive buffer has space for 256 chars). The last character from
	  request will be dropped by cwdaemon's receive code.

	  The count of bytes in the sent request includes terminating NUL.
	*/
	{ .description = "plain request with size larger than cwdaemon's receive buffer - 257+1 bytes (with NUL); TRUNCATION of Morse receive",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "1234567\0"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "123456",
	},
};




static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_run(test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver, events_t * events);
static int evaluate_events(events_t * events, const test_case_t * test_case);




int request_size_test(void)
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

	if (0 != test_run(g_test_cases, n_test_cases, &client, &morse_receiver, &events)) {
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




	/* Define set of expected events. */
	event_type_t expected_events[EVENTS_MAX] = { 0 };
	int n_expected = 0; /* Count of expected events. */
	expected_events[n_expected++] = event_type_morse_receive;




	expectation_idx = 1;
	if (0 != expect_count_of_events(expectation_idx, events->event_idx, n_expected)) {
		return -1;
	}




	/* Expectation: correct types and order of events. */
	expectation_idx = 2;
	const event_t * morse_event = NULL;
	for (int i = 0; i < n_expected; i++) {
		if (expected_events[i] != events->events[i].event_type) {
			test_log_err("Expectation %d: unexpected event %d at position %d\n", expectation_idx, events->events[i].event_type, i);
			return -1;
		}

		/* Get references to specific events in array of events. */
		switch (events->events[i].event_type) {
		case event_type_morse_receive:
			morse_event = &events->events[i];
			break;
		case event_type_none:
		case event_type_client_socket_receive:
		case event_type_request_exit:
		case event_type_sigchld:
		default:
			test_log_err("Expectation %d: unhandled event type %d at position %d\n", expectation_idx, events->events[i].event_type, i);
			return -1;
		}
	}
	test_log_info("Expectation %d: found expected types of events, in proper order\n", expectation_idx);




	expectation_idx = 3;
	if (0 != expect_morse_receive_match(expectation_idx, morse_event->u.morse_receive.string, test_case->expected_morse_receive)) {
		return -1;
	}




	test_log_info("Test: evaluation of test events was successful %s\n", "");

	return 0;
}




/**
   @brief Prepare resources used to execute set of test cases
*/
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver)
{
	bool failure = false;

	/* There is a lot of characters in test cases. Let's play them quickly to
	   make the test short. */
	const int wpm = TEST_WPM_MAX;

	/* Prepare local test instance of cwdaemon server. */
	const cwdaemon_opts_t cwdaemon_opts = {
		//.supervisor_id =  supervisor_id_valgrind,
		.tone           = TEST_TONE_EASY,
		.sound_system   = CW_AUDIO_SOUNDCARD,
		.nofork         = true,
		.cwdevice_name  = TEST_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
	};
	if (0 != server_start(&cwdaemon_opts, server)) {
		test_log_err("Test: failed to start cwdaemon server %s\n", "");
		failure = true;
	}


	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.01.24: remove casting. */
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

			/* Send the message to be played. Notice that we use
			   test_case->plain_request.n_bytes to specify count of bytes to
			   be sent. */
			client_send_message(client, test_case->plain_request.bytes, test_case->plain_request.n_bytes);

			morse_receiver_wait(morse_receiver);
		}

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




