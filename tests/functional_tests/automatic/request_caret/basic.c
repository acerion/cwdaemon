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
	const char * description;                 /**< Tester-friendly description of test case. */
	const char * full_message;                /**< Text to be sent to cwdaemon server in the MESSAGE request. Full message, so it SHOULD include caret. */
	socket_receive_data_t expected_socket_reply;  /**< What is expected to be received through socket from cwdaemon server. Full reply, so it SHOULD include terminating "\r\n". */
	const char * expected_morse_receive;      /**< What is expected to be received by Morse code receiver (without ending space). */
} test_case_t;




/*
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
static const char err_case_1_message[]                = { 'p', 'a', 's', 's', 'e', 'n', -1, 'e', 'r', '^', '\0' };         /* Notice inserted -1' */
static const char err_case_1_expected_morse_receive[] = { 'p', 'a', 's', 's', 'e', 'n',     'e', 'r', '\0' };              /* Morse message keyed on cwdevice must not contain the -1 char (the char should be skipped by cwdaemon). */





static test_case_t g_test_cases[] = {
	{ .description = "mixed characters",
	  .full_message               = "22 crows, 1 stork?^",
	  .expected_socket_reply      = SOCKET_BUF_SET("22 crows, 1 stork?\r\n"),
	  .expected_morse_receive     = "22 crows, 1 stork?",
	},



	/*
	  Handling of caret in cwdaemon indicates that once a first caret is
	  recognized, the caret and everything after it is ignored:

	      case '^':
	          *x = '\0';     // Remove '^' and possible trailing garbage.
	*/
	{ .description = "additional message after caret",
	  .full_message               = "Fun^Joy^",
	  .expected_socket_reply      = SOCKET_BUF_SET("Fun\r\n"),
	  .expected_morse_receive     = "Fun",
	},
	{ .description = "message with two carets",
	  .full_message               = "Monday^^",
	  .expected_socket_reply      = SOCKET_BUF_SET("Monday\r\n"),
	  .expected_morse_receive     = "Monday",
	},



	{ .description = "two words",
	  .full_message               = "Hello world!^",
	  .expected_socket_reply      = SOCKET_BUF_SET("Hello world!\r\n"),
	  .expected_morse_receive     = "Hello world!",
	},

	/* There should be no action from cwdaemon: neither keying nor socket
	   reply. */
	{ .description = "empty text",
	  .full_message               = "^",
	  .expected_socket_reply      = SOCKET_BUF_SET(""),
	  .expected_morse_receive     = "",
	},

	{ .description = "single character",
	  .full_message               = "f^",
	  .expected_socket_reply      = SOCKET_BUF_SET("f\r\n"),
	  .expected_morse_receive     = "f",
	},

	{ .description = "single word",
	  .full_message               = "Paris^",
	  .expected_socket_reply      = SOCKET_BUF_SET("Paris\r\n"),
	  .expected_morse_receive     = "Paris",
	},

	/* Notice how the leading space from message is preserved in socket reply. */
	{ .description = "single word with leading space",
	  .full_message               = " London^",
	  .expected_socket_reply      = SOCKET_BUF_SET(" London\r\n"),
	  .expected_morse_receive     = "London",
	},

	/* Notice how the trailing space from message is preserved in socket reply. */
	{ .description = "mixed characters with trailing space",
	  .full_message               = "when, now = right: ^",
	  .expected_socket_reply      = SOCKET_BUF_SET("when, now = right: \r\n"),
	  .expected_morse_receive     = "when, now = right:",
	},

	{ .description                = "message containing '-1' integer value",
	  .full_message               = err_case_1_message,
	  .expected_socket_reply      = { .n_bytes = 11, .bytes = { 'p', 'a', 's', 's', 'e', 'n', -1, 'e', 'r', '\r', '\n' } },      /* cwdaemon sends verbatim text in socket reply. */
	  .expected_morse_receive     = err_case_1_expected_morse_receive,
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




	/*
	  Expectation 1: in most cases there should be 2 events:
	   - Receiving some reply over socket from cwdaemon server,
	   - Receiving some Morse code on cwdevice.
	  In other cases there should be zero events.

	  The two events go hand in hand: if one is expected, the other is too.
	  If one is not expected to occur, the other is not expected either.
	*/
	expectation_idx++;
	const bool expecting_morse_event = 0 != strlen(test_case->expected_morse_receive);
	const bool expecting_socket_reply_event = 0 != test_case->expected_socket_reply.n_bytes;
	if (!expecting_morse_event && !expecting_socket_reply_event) {
		if (0 != events->event_idx) {
			test_log_err("Expectation 1: incorrect count of events recorded. Expected 0 events, got %d\n", events->event_idx);
			return -1;
		}
		test_log_info("Expectation 1: there are zero events (as expected), so evaluation of events is now completed %s\n", "");
		return 0;
	} else if (expecting_morse_event && expecting_socket_reply_event) {
		if (2 != events->event_idx) {
			test_log_err("Expectation 1: incorrect count of events recorded. Expected 2 events, got %d\n", events->event_idx);
			return -1;
		}
	} else {
		test_log_err("Expectation 1: Incorrect situation when checking 'expecting' flags: %d != %d\n", expecting_morse_event, expecting_socket_reply_event);
		return -1;
	}
	test_log_info("Expectation 1: count of events is correct: %d\n", events->event_idx);




	/*
	  Expectation 2: events are of correct type.
	*/
	expectation_idx++;
	int morse_idx = -1;
	const int morse_cnt  = events_find_by_type(events, event_type_morse_receive, &morse_idx);
	if (1 != morse_cnt) {
		test_log_err("Expectation 2: incorrect count of Morse receive events: expected 1, got %d\n", morse_cnt);
		return -1;
	}
	const event_t * morse_event = &events->events[morse_idx];

	int socket_idx = -1;
	const int socket_cnt = events_find_by_type(events, event_type_client_socket_receive, &socket_idx);
	if (1 != socket_cnt) {
		test_log_err("Expectation 2: incorrect count of socket receive events: expected 1, got %d\n", socket_cnt);
		return -1;
	}
	const event_t * socket_event = &events->events[socket_idx];

	test_log_info("Expectation 2: types of events are correct %s\n", "");




	expectation_idx++;
	if (0 != expect_morse_and_socket_event_order(expectation_idx, morse_idx, socket_idx)) {
		return -1;
	}




	expectation_idx++;
	if (0 != expect_socket_reply_match(expectation_idx, &socket_event->u.socket_receive, &test_case->expected_socket_reply)) {
		return -1;
	}




	/*
	  Receiving of message by Morse receiver should not be verified if the
	  expected message is too short (the problem with "warm-up" of receiver).
	  TOOO acerion 2024.01.28: remove "do_morse_test" flag after fixing
	  receiver.
	*/
	expectation_idx++;
	const bool do_morse_test = strlen(test_case->expected_morse_receive) > 1;
	if (do_morse_test) {
		if (0 != expect_morse_receive_match(expectation_idx, morse_event->u.morse_receive.string, test_case->expected_morse_receive)) {
			return -1;
		}
	} else {
		test_log_notice("Expectation %d: skipping verification of message received by Morse receiver due to short expected string\n", expectation_idx);
	}




	expectation_idx++;
	if (0 != expect_morse_and_socket_events_distance(expectation_idx, morse_idx, morse_event, socket_idx, socket_event)) {
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

	const int wpm = test_get_test_wpm();

	/* Prepare local test instance of cwdaemon server. */
	const cwdaemon_opts_t cwdaemon_opts = {
		//.supervisor_id =  supervisor_id_valgrind,
		.tone           = test_get_test_tone(),
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

			/* Send the message to be played. */
			client_send_message(client, test_case->full_message, strlen(test_case->full_message) + 1);

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




