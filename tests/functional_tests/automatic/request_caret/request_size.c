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
   Test cases that send to cwdaemon caret requests that have size (count of
   characters) close to cwdaemon's maximum size of requests. The requests are
   slightly smaller, equal to and slightly larger than the size of cwdaemon's
   buffer..

   cwdaemon's buffer used to receive network messages has
   CWDAEMON_MESSAGE_SIZE_MAX bytes. If a caret request sent to cwdaemon is
   larger than that, it will be truncated.
*/




#define _DEFAULT_SOURCE




#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "request_size.h"

#include "tests/library/events.h"
#include "tests/library/log.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/server.h"
#include "tests/library/socket.h"
#include "tests/library/string_utils.h"
#include "tests/library/test_env.h"
#include "tests/library/time_utils.h"




typedef struct test_case_t {
	const char * description;                 /**< Tester-friendly description of test case. */
	const char * caret_request;               /**< Text to be sent to cwdaemon server in the MESSAGE request. Full message, so it SHOULD include caret. */
	const size_t n_bytes;                     /**< How many bytes of plain requests to send? */
	const char * expected_morse_receive;      /**< What is expected to be received by Morse code receiver (without ending space). */
	const char * expected_socket_reply;       /**< What is expected to be received through socket from cwdaemon server. Full reply, so it SHOULD include terminating "\r\n". */
} test_case_t;




/* Helper definition to shorten strings in test cases. Chars at position 51
   till 250, inclusive. */
#define CHARS_51_250 \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku"




static test_case_t g_test_cases[] = {
	{ .description = "caret request with size smaller than cwdaemon's receive buffer - 255 chars, not including NUL",
	  .caret_request          =         "paris kukukukukukukukukukukukukukukukukukukukukuku" CHARS_51_250 "1234^",
	  .n_bytes                = sizeof ("paris kukukukukukukukukukukukukukukukukukukukukuku" CHARS_51_250 "1234^") - 1, /* -1 to avoid sending terminating NUL. */
	  .expected_morse_receive =         "paris kukukukukukukukukukukukukukukukukukukukukuku" CHARS_51_250 "1234",
	  .expected_socket_reply  =         "paris kukukukukukukukukukukukukukukukukukukukukuku" CHARS_51_250 "1234\r\n",
	},

	{ .description = "caret request with size equal to cwdaemon's receive buffer - 256 chars, not including NUL",
	  .caret_request          =         "paris kukukukukukukukukukukukukukukukukukukukukuku" CHARS_51_250 "12345^",
	  .n_bytes                = sizeof ("paris kukukukukukukukukukukukukukukukukukukukukuku" CHARS_51_250 "12345^") - 1, /* -1 to avoid sending terminating NUL. */
	  .expected_morse_receive =         "paris kukukukukukukukukukukukukukukukukukukukukuku" CHARS_51_250 "12345",
	  .expected_socket_reply  =         "paris kukukukukukukukukukukukukukukukukukukukukuku" CHARS_51_250 "12345\r\n",
	},

	{ .description = "caret request with size larger than cwdaemon's receive buffer - 257 chars, not including NUL",
	  .caret_request          =         "paris kukukukukukukukukukukukukukukukukukukukukuku" CHARS_51_250 "123456^",
	  .n_bytes                = sizeof ("paris kukukukukukukukukukukukukukukukukukukukukuku" CHARS_51_250 "123456^") - 1, /* -1 to avoid sending terminating NUL. */
	  .expected_morse_receive =         "paris kukukukukukukukukukukukukukukukukukukukukuku" CHARS_51_250 "123456",
	  /* '^' was at position 257, so it will be dropped by cwdaemon's receive code.
	     cwdaemon won't interpret this request as caret request, and won't send
	     anything over socket. */
	  .expected_socket_reply  =         "",
	},
};




static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_run(test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver, events_t * events);
static int evaluate_events(events_t * events, const test_case_t * test_case);




int request_size_caret_test(void)
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


	/*
	  Expectation 1: in most cases there should be 2 events:
	   - Receiving some reply over socket from cwdaemon server,
	   - Receiving some Morse code on cwdevice.
	  In other cases there should be one event: just the Morse event.
	*/
	const bool expecting_socket_reply_event = 0 != strlen(test_case->expected_socket_reply);
	if (expecting_socket_reply_event) {
		if (2 != events->event_idx) {
			test_log_err("Expectation 1: incorrect count of events recorded. Expected 2 events, got %d\n", events->event_idx);
			return -1;
		}
	} else {
		if (1 != events->event_idx) {
			test_log_err("Expectation 1: incorrect count of events recorded. Expected 1 event, got %d\n", events->event_idx);
			return -1;
		}
	}
	test_log_info("Expectation 1: count of events is correct: %d\n", events->event_idx);




	/*
	  Expectation 2: events are of correct type.
	*/
	const event_t * morse_event = NULL;
	const event_t * socket_event = NULL;
	int morse_idx = -1;
	int socket_idx = -1;

	if (events->events[0].event_type == events->events[1].event_type) {
		test_log_err("Expectation 2: both events have the same type %s\n", "");
		return -1;
	}

	for (int i = 0; i < events->event_idx; i++) {
		if (events->events[i].event_type == event_type_client_socket_receive) {
			socket_event = &events->events[i];
			socket_idx = i;
		} else if (events->events[i].event_type == event_type_morse_receive) {
			morse_event = &events->events[i];
			morse_idx = i;
		} else {
			test_log_err("Expectation 2: found unexpected event type at position %d: %d\n", i, events->events[i].event_type);
			return -1;
		}
	}
	if (expecting_socket_reply_event != (NULL != socket_event)) {
		test_log_err("Expectation 2: invalid condition for socket reply event: expected = %d, is present = %d\n", expecting_socket_reply_event, (NULL != socket_event));
		return -1;
	}
	test_log_info("Expectation 2: found expected event types %s\n", "");




	/*
	  Expectation 3: events in proper order.

	  I'm not 100% sure what the correct order should be in perfect
	  implementation of cwdaemon. In 0.12.0 it is "socket receive" first, and
	  then "morse receive" second, unless a message sent to server ends with
	  space.

	  TODO acerion 2024.01.28: check what SHOULD be the correct order of the
	  two events. Some comments in cwdaemon indicate that reply should be
	  sent after end of playing Morse.

	  TODO acerion 2024.01.26: Double check the corner case with space at the
	  end of message.
	*/
	if (expecting_socket_reply_event) {
		if (morse_idx < socket_idx) {
			test_log_warn("Expectation 3: unexpected order of events: Morse first (idx = %d), socket second (idx = %d)\n", morse_idx, socket_idx);
			//return -1; /* TODO acerion 2024.01.26: uncomment after your are certain of correct order of events. */
		} else {
			test_log_info("Expectation 3: expected order of events: socket first (idx = %d), Morse second (idx = %d)\n", socket_idx, morse_idx);
		}
	} else {
		test_log_info("Expectation 3: skipping checking order of events if there is only one event %s\n", "");
	}




	/*
	  Expectation 4: cwdaemon client has received over socket a correct
	  reply.
	*/
	if (expecting_socket_reply_event) {
		const char * full_expected = test_case->expected_socket_reply;
		char printable_expected[PRINTABLE_BUFFER_SIZE(sizeof (full_expected))] = { 0 };
		get_printable_string(full_expected, printable_expected, sizeof (printable_expected));

		const char * full_received = socket_event->u.socket_receive.string;
		char printable_received[PRINTABLE_BUFFER_SIZE(sizeof (full_received))] = { 0 };
		get_printable_string(full_received, printable_received, sizeof (printable_received));

		if (0 != strcmp(full_received, full_expected)) {
			test_log_err("Expectation 4: received socket reply [%s] doesn't match expected socket reply [%s]\n", printable_received, printable_expected);
			return -1;
		}
		test_log_info("Expectation 4: received socket reply [%s] matches expected reply [%s]\n", printable_received, printable_expected);
	} else {
		test_log_info("Expectation 4: skipping checking contents of socket reply because there is no socket event %s\n", "");
	}





	/*
	  Expectation 5: cwdaemon keyed a proper Morse message on cwdevice.

	  Receiving of message by Morse receiver should not be verified if the
	  expected message is too short (the problem with "warm-up" of receiver).
	  TOOO acerion 2024.01.28: remove "do_morse_test" flag after fixing
	  receiver.
	*/
	if (!morse_receive_text_is_correct(morse_event->u.morse_receive.string, test_case->expected_morse_receive)) {
		test_log_err("Expectation 5: received Morse message [%s] doesn't match expected receive [%s]\n",
		             morse_event->u.morse_receive.string, test_case->expected_morse_receive);
		return -1;
	}
	test_log_info("Expectation 5: received Morse message [%s] matches expected receive [%s] (ignoring the first character)\n",
	              morse_event->u.morse_receive.string, test_case->expected_morse_receive);




	/*
	  Expectation 6: "Morse receive" event and "socket receive" event are
	  close to each other.

	  TODO acerion 2024.01.26: the threshold is still 0.5 seconds. That's a
	  lot. Try to bring it down. Notice that the time diff may depend on
	  Morse code speed (wpm).
	*/
	if (expecting_socket_reply_event) {
		struct timespec diff = { 0 };
		if (morse_idx < socket_idx) {
			timespec_diff(&morse_event->tstamp, &socket_event->tstamp, &diff);
		} else {
			timespec_diff(&socket_event->tstamp, &morse_event->tstamp, &diff);
		}

		const int threshold = 500L * 1000 * 1000; /* [nanoseconds] */
		if (diff.tv_sec > 0 || diff.tv_nsec > threshold) {
			test_log_err("Expectation 6: time difference between end of 'Morse receive' and receiving socket reply is too large: %ld.%09ld seconds\n", diff.tv_sec, diff.tv_nsec);
			return -1;
		}
		test_log_info("Expectation 6: time difference between end of 'Morse receive' and receiving socket reply is ok: %ld.%09ld seconds\n", diff.tv_sec, diff.tv_nsec);
	} else {
		test_log_info("Expectation 6: skipping checking of the expectation because socket event is not present %s\n", "");
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
	   make the test short. Using "25" because at "30" there are too many
	   errors. */
	const int wpm = 25;

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
			   test_case->n_bytes to specify count of bytes to be sent. */
			client_send_message(client, test_case->caret_request, test_case->n_bytes);

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




