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
   Test of "<ESC>h request" feature: prepare a reply to be sent by cwdaemon.
*/




#define _DEFAULT_SOURCE




#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tests/library/client.h"
#include "tests/library/events.h"
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




typedef struct test_case_t {
	const char * description;                /** Human-readable description of the test case. */
	const char full_message[128];            /** Full text of message to be played by cwdaemon. */
	const char expected_morse_receive[128];  /**< What is expected to be received by Morse code receiver (without ending space). */
	const char requested_reply_value[128];   /**< What is being sent to cwdaemon server as expected value of reply (without leading 'h'). */
} test_case_t;




static test_case_t g_test_cases[] = {
	/* This is a SUCCESS case. We request cwdaemon server to send us empty
	   string in reply. */
	{ .description             = "success case, empty reply value",
	  .full_message            = "paris",
	  .expected_morse_receive  = "paris",
	  .requested_reply_value   = "",
	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   single-letter string in reply. */
	{ .description             = "success case, single-letter as a value of reply",
	  .full_message            = "paris",
	  .expected_morse_receive  = "paris",
	  .requested_reply_value   = "r",
	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   single-word string in reply. */
	{ .description             = "success case, a word as value of reply",
	  .full_message            = "paris",
	  .expected_morse_receive  = "paris",
	  .requested_reply_value   = "reply",
	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   full-sentence string in reply. */
	{ .description             = "success case, a sentence as a value of reply",
	  .full_message            = "paris",
	  .expected_morse_receive  = "paris",
	  .requested_reply_value   = "This is a reply to your 27th request.",
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
	  .full_message            = { 'p', 'a', 's', 's', 'e', 'n', -1, 'e', 'r', '\0' },  /* Notice inserted -1' */
	  .expected_morse_receive  = { 'p', 'a', 's', 's', 'e', 'n',     'e', 'r', '\0' },  /* Morse message keyed on cwdevice must not contain the -1 char (the char should be skipped by cwdaemon). */
	  .requested_reply_value   = { 'l', -1,  'z', 'a', 'r', 'd', '\0' },                /* cwdaemon doesn't validate values of chars that are requested for socket reply. */
	},
};




static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_run(test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver, events_t * events);
static int evaluate_events(events_t * events, const test_case_t * test_case);




int main(void)
{
#if 0
	if (!test_env_is_usable(test_env_libcw_without_signals)) {
		test_log_err("Test: preconditions for test env are not met, exiting %s\n", "");
		exit(EXIT_FAILURE);
	}
#endif

	const uint32_t seed = cwdaemon_srandom(0);
	test_log_debug("Test: random seed: 0x%08x (%u)\n", seed, seed);

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
		test_log_err("Test: the test has failed %s\n", "");
		exit(EXIT_FAILURE);
	} else {
		test_log_info("Test: the test has passed %s\n", "");
		exit(EXIT_SUCCESS);
	}
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


	const event_t * event_0 = &events->events[0];
	const event_t * event_1 = &events->events[1];
	const event_t * morse_event = NULL;
	const event_t * socket_event = NULL;




	/* Expectation 1: there should be only two events. */
	{
		if (2 != events->event_idx) {
			test_log_err("Expectation 1: unexpected count of events: %d (expected 2)\n", events->event_idx);
			return -1;
		}
	}
	test_log_info("Expectation 1: count of events is correct: %d\n", events->event_idx);




	/* Expectation 2: first event should be Morse receive, second event should be reply on socket. */
	{
		if (event_0->event_type == event_type_morse_receive
		    && event_1->event_type != event_type_client_socket_receive) {

			/*
			  This would be the correct order of events, but currently
			  (cwdaemon 0.11.0, 0.12.0) this is not the case: the order of
			  events is reverse. Right now I'm not willing to fix it yet.

			  TODO acerion 2023.12.30: fix the order of the two events in
			  cwdaemon. At the very least decrease the time difference
			  between the events from current ~300ms to few ms.
			*/
			morse_event = event_0;
			socket_event = event_1;
			; /* Pass. */
		} else {
			if (event_0->event_type == event_type_client_socket_receive
			    && event_1->event_type == event_type_morse_receive) {

				// This is the current incorrect behaviour that is accepted
				// for now.
				test_log_warn("Expectation 2: incorrect (but currently expected) order of events: %d -> %d\n",
				              event_0->event_type, event_1->event_type);
				socket_event = event_0;
				morse_event = event_1;
				; /* Pass. */
			} else {
				test_log_err("Expectation 2: completely incorrect order of events: %d -> %d\n",
				             event_0->event_type, event_1->event_type);
				return -1;
			}
		}
	}
	test_log_info("Expectation 2: types of events are correct %s\n", "");




	/* Expectation 3: the events should be separated by close time span. */
	/* Maximal allowed time span between the two events. Currently (0.12.0)
	   the time span is ~300ms.
	   TODO acerion 2023.12.31: shorten the time span. */
	const long int threshold = (long) 500 * 1000 * 1000;
	struct timespec diff = { 0 };
	timespec_diff(&event_0->tstamp, &event_1->tstamp, &diff);
	if (diff.tv_sec > 0 || diff.tv_nsec > threshold) {
		test_log_err("Expectation 3: time difference between end of 'Morse receive' and receiving socket reply is too large: %ld.%09ld seconds\n",
		             diff.tv_sec, diff.tv_nsec);
		return -1;
	}
	test_log_info("Expectation 3: time difference between end of 'Morse receive' and receiving socket reply is ok: %ld.%09ld seconds\n", diff.tv_sec, diff.tv_nsec);




	/* Expectation 4: text received by Morse receiver must match input text from test case.

	   While this is not THE feature that needs to be verified by this test,
	   it's good to know that we received full and correct data. */
	{
		const char * received = morse_event->u.morse_receive.string;
		const char * expected = test_case->expected_morse_receive;
		if (!morse_receive_text_is_correct(received, expected)) {
			test_log_err("Expectation 4: received Morse message [%s] doesn't match text from message request [%s]\n",
			             received, expected);
			return -1;
		}
		test_log_info("Expectation 4: received Morse message [%s] matches expected Morse message [%s] (ignoring the first character)\n",
		              received, expected);
	}




	/* Expectation 5: text received in socket message must match text sent in <ESC>h
	   request. */
	{
		const char * actual_raw = socket_event->u.socket_receive.string;

		char expected_raw[64] = { 0 };
		snprintf(expected_raw, sizeof (expected_raw), "h%s\r\n", test_case->requested_reply_value);

		char escaped_expected[64] = { 0 };
		char escaped_actual[64] = { 0 };
		escape_string(expected_raw, escaped_expected, sizeof (escaped_expected));
		escape_string(actual_raw, escaped_actual, sizeof (escaped_actual));

		if (0 != strcmp(expected_raw, actual_raw)) {
			test_log_err("Expectation 5: received incorrect message in socket reply: expected [%s], received [%s]\n",
			             escaped_expected, escaped_actual);
			return -1;
		}

		test_log_info("Expectation 5: received correct message in socket reply: expected [%s], received [%s]\n",
		              escaped_expected, escaped_actual);
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

	int wpm = 10;
	/* Remember that some receive timeouts in tests were selected when the
	   wpm was hardcoded to 10 wpm. Picking values lower than 10 may lead to
	   overrunning the timeouts. */
	cwdaemon_random_uint(10, 15, (unsigned int *) &wpm);


	/* Prepare local test instance of cwdaemon server. */
	cwdaemon_opts_t server_opts = {
		.tone           = 700,
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
		test_case_t * test_case = &test_cases[i];

		test_log_newline(); /* Visual separator. */
		test_log_info("Test: starting test case %zd/%zd: [%s]\n", i + 1, n_test_cases, test_case->description);

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
			client_send_esc_request(client, CWDAEMON_ESC_REQUEST_REPLY, test_case->requested_reply_value, strlen(test_case->requested_reply_value) + 1);

			/* Send the message to be played. */
			client_send_message(client, test_case->full_message, strlen(test_case->full_message) + 1);


			morse_receiver_wait(morse_receiver);
		}


		/* Validation of test run. */
		if (0 != evaluate_events(events, test_case)) {
			test_log_err("Test: evaluation of events has failed for test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			break;
		}
		/* Clear stuff before running next test case. */
		events_clear(events);


		test_log_info("Test: test case %zd/%zd succeeded\n\n", i + 1, n_test_cases);
	}

	return failure ? -1 : 0;
}

