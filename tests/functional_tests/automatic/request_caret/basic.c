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
   Basic tests of caret ('^') request.

   The tests are basic because a single test case just sends one caret
   requests and sees what happens.

   TODO acerion 2024.01.26: add "advanced" tests (in separate file) in which
   there is some client code that waits for server's response and interacts
   with it, perhaps by sending another caret request, and then another, and
   another. Come up with some good methods of testing of more advanced
   scenarios.
*/




#define _DEFAULT_SOURCE




#include "config.h"

/* For kill() on FreeBSD 13.2 */
//#include <signal.h>
//#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <unistd.h>

#include "basic.h"

//#include "tests/library/cwdevice_observer.h"
//#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/events.h"
#include "tests/library/expectations.h"
#include "tests/library/log.h"
//#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/server.h"
#include "tests/library/socket.h"
#include "tests/library/string_utils.h"
#include "tests/library/test_env.h"
//#include "tests/library/thread.h"
#include "tests/library/time_utils.h"




typedef struct test_case_t {
	const char * description;                            /**< Tester-friendly description of test case. */
	socket_send_data_t caret_request;                    /**< Text to be sent to cwdaemon server in the MESSAGE request. Full message, so it SHOULD include caret. */
	socket_receive_data_t expected_socket_reply;  /**< What is expected to be received through socket from cwdaemon server. Full reply, so it SHOULD include terminating "\r\n". */
	const char expected_morse_receive[400];      /**< What is expected to be received by Morse code receiver (without ending space). */
} test_case_t;




/*
  Info for test case with '-1' byte.

  Data for testing how cwdaemon handles a bug in libcw.

  libcw 8.0.0 from unixcw 3.6.1 crashes when enqueued character has value
  ((char) -1) / ((unsigned char) 255). This has been fixed in unixcw commit
  c4fff9622c4e86c798703d637be7cf7e9ab84a06.

  Since cwdaemon has to still work with unfixed versions of library, it has
  to skip (not enqueue) the character.

  The problem is worked-around in cwdaemon by adding 'is_valid' condition
  before calling cw_send_character().

  TODO acerion 2024.02.18: this functional test doesn't display information
  that cwdaemon which doesn't have a workaround is experiencing a crash. It
  would be good to know in all functional tests that cwdaemon has crashed -
  it would give more info to tester.

  TODO acerion 2024.02.18: make sure that the description of caret message
  contains the information that socket reply includes all characters from
  original message, including invalid characters that weren't keyed on
  cwdevice.

  TODO acerion 2024.02.18: make sure that similar test is added for
  regular/plain message requests in the future.
*/





static test_case_t g_test_cases[] = {
	{ .description = "mixed characters",
	  .caret_request              = SOCKET_BUF_SET("22 crows, 1 stork?^"),
	  .expected_socket_reply      = SOCKET_BUF_SET("22 crows, 1 stork?\r\n"),
	  .expected_morse_receive     =                "22 crows, 1 stork?",
	},



	/*
	  Handling of caret in cwdaemon indicates that once a first caret is
	  recognized, the caret and everything after it is ignored:

	      case '^':
	          *x = '\0';     // Remove '^' and possible trailing garbage.
	*/
	{ .description = "additional message after caret",
	  .caret_request              = SOCKET_BUF_SET("Fun^Joy^"),
	  .expected_socket_reply      = SOCKET_BUF_SET("Fun\r\n"),
	  .expected_morse_receive     =                "Fun",
	},
	{ .description = "message with two carets",
	  .caret_request              = SOCKET_BUF_SET("Monday^^"),
	  .expected_socket_reply      = SOCKET_BUF_SET("Monday\r\n"),
	  .expected_morse_receive     =                "Monday",
	},



	{ .description = "two words",
	  .caret_request              = SOCKET_BUF_SET("Hello world!^"),
	  .expected_socket_reply      = SOCKET_BUF_SET("Hello world!\r\n"),
	  .expected_morse_receive     =                "Hello world!",
	},

	/* There should be no action from cwdaemon: neither keying nor socket
	   reply. */
	{ .description = "empty text - no terminating NUL in request",
	  .caret_request              = SOCKET_BUF_SET("^"),
	  .expected_socket_reply      = SOCKET_BUF_SET(""),
	  .expected_morse_receive     =                "",
	},

	/* There should be no action from cwdaemon: neither keying nor socket
	   reply. */
	{ .description = "empty text - with terminating NUL in request",
	  .caret_request              = SOCKET_BUF_SET("^\0"), /* Explicit terminating NUL. The NUL will be ignored by cwdaemon. */
	  .expected_socket_reply      = SOCKET_BUF_SET(""),
	  .expected_morse_receive     =                "",
	},

	{ .description = "single character",
	  .caret_request              = SOCKET_BUF_SET("f^"),
	  .expected_socket_reply      = SOCKET_BUF_SET("f\r\n"),
	  .expected_morse_receive     =                "f",
	},

	{ .description = "single word - no terminating NUL in request",
	  .caret_request              = SOCKET_BUF_SET("Paris^"),
	  .expected_socket_reply      = SOCKET_BUF_SET("Paris\r\n"),
	  .expected_morse_receive     =                "Paris",
	},

	{ .description = "single word - with terminating NUL in request",
	  .caret_request              = SOCKET_BUF_SET("Paris^\0"), /* Explicit terminating NUL. The NUL will be ignored by cwdaemon. */
	  .expected_socket_reply      = SOCKET_BUF_SET("Paris\r\n"),
	  .expected_morse_receive     =                "Paris",
	},

	/* Notice how the leading space from message is preserved in socket reply. */
	{ .description = "single word with leading space",
	  .caret_request              = SOCKET_BUF_SET(" London^"),
	  .expected_socket_reply      = SOCKET_BUF_SET(" London\r\n"),
	  .expected_morse_receive     =                 "London",
	},

	/* Notice how the trailing space from message is preserved in socket reply. */
	{ .description = "mixed characters with trailing space",
	  .caret_request              = SOCKET_BUF_SET("when, now = right: ^"),
	  .expected_socket_reply      = SOCKET_BUF_SET("when, now = right: \r\n"),
	  .expected_morse_receive     =                "when, now = right:",
	},

	/* Refer to comment starting with "Info for test case with '-1' byte."
	   above for more info. */
	{ .description                = "message containing '-1' integer value",
	  .caret_request              = { .n_bytes = 10, .bytes = { 'p', 'a', 's', 's', 'e', 'n', -1, 'e', 'r', '^', } },
	  .expected_socket_reply      = { .n_bytes = 11, .bytes = { 'p', 'a', 's', 's', 'e', 'n', -1, 'e', 'r', '\r', '\n' } },      /* cwdaemon sends verbatim text in socket reply. */
	  .expected_morse_receive     =                           { 'p', 'a', 's', 's', 'e', 'n',     'e', 'r', '\0' },              /* Morse message keyed on cwdevice must not contain the -1 char (the char should be skipped by cwdaemon). */
	},
};




static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_run(test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver, events_t * events);
static int evaluate_events(events_t * events, const test_case_t * test_case);




int basic_caret_test(void)
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




	/* Expectation: correct count of events. */
	expectation_idx = 1;
	const bool expecting_morse_event = 0 != strlen(test_case->expected_morse_receive);
	const bool expecting_socket_reply_event = 0 != test_case->expected_socket_reply.n_bytes;
	int expected_cnt = 0;
	if (expecting_morse_event) {
		expected_cnt++;
	}
	if (expecting_socket_reply_event) {
		expected_cnt++;
	}
	if (expected_cnt != events->event_idx) {
		test_log_err("Expectation %d: incorrect count of events recorded. Expected %d events, got %d\n", expectation_idx, expected_cnt, events->event_idx);
		return -1;
	}
	if (0 == events->event_idx) {
		test_log_info("Expectation %d: there are zero events (as expected), so evaluation of events is now completed\n", expectation_idx);
		return 0;
	}
	test_log_info("Expectation %d: count of events is correct: %d\n", expectation_idx, events->event_idx);




	/*
	  Expectation: events are of correct type.
	*/
	expectation_idx = 2;
	int morse_idx = -1;
	const int expected_morse_cnt = expecting_morse_event ? 1 : 0;
	const int morse_cnt  = events_find_by_type(events, event_type_morse_receive, &morse_idx);
	if (expected_morse_cnt != morse_cnt) {
		test_log_err("Expectation %d: incorrect count of Morse receive events: expected 1, got %d\n", expectation_idx, morse_cnt);
		return -1;
	}
	const event_t * morse_event = &events->events[morse_idx];

	int socket_idx = -1;
	const int socket_cnt = events_find_by_type(events, event_type_client_socket_receive, &socket_idx);
	const int expected_socket_cnt = expecting_socket_reply_event ? 1 : 0;
	if (expected_socket_cnt != socket_cnt) {
		test_log_err("Expectation %d: incorrect count of socket receive events: expected %d, got %d\n", expectation_idx, expected_socket_cnt, socket_cnt);
		return -1;
	}
	const event_t * socket_event = &events->events[socket_idx];

	test_log_info("Expectation %d: types of events are correct\n", expectation_idx);




	expectation_idx = 3;
	if (expecting_socket_reply_event) {
		if (0 != expect_morse_and_socket_event_order(expectation_idx, morse_idx, socket_idx)) {
			return -1;
		}
	} else {
		test_log_info("Expectation %d: skipping checking order of events if there is only one event\n", expectation_idx);
	}




	expectation_idx = 4;
	if (expecting_socket_reply_event) {
		if (0 != expect_socket_reply_match(expectation_idx, &socket_event->u.socket_receive, &test_case->expected_socket_reply)) {
			return -1;
		}
	} else {
		test_log_info("Expectation %d: skipping checking contents of socket reply because there is no socket event\n", expectation_idx);
	}




	expectation_idx = 5;
	if (expecting_morse_event) {
		if (0 != expect_morse_receive_match(expectation_idx, morse_event->u.morse_receive.string, test_case->expected_morse_receive)) {
			return -1;
		}
	} else {
		test_log_notice("Expectation %d: skipping verification of message received by Morse receiver due to short expected string\n", expectation_idx);
	}




	expectation_idx = 6;
	if (expecting_socket_reply_event) {
		if (0 != expect_morse_and_socket_events_distance(expectation_idx, morse_idx, morse_event, socket_idx, socket_event)) {
			return -1;
		}
	} else {
		test_log_info("Expectation %d: skipping checking of the expectation because socket event is not present\n", expectation_idx);
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

	/* There may be a lot of characters in test cases. Let's play them quickly to
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
			   test_case->caret_request.n_bytes_to_send to specify count of
			   bytes to be sent. */
			client_send_message(client, test_case->caret_request.bytes, test_case->caret_request.n_bytes);

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




