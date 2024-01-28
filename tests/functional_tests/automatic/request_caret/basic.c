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
#include <signal.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "basic.h"

#include "tests/library/cwdevice_observer.h"
#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/events.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/process.h"
#include "tests/library/random.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"
#include "tests/library/thread.h"
#include "tests/library/time_utils.h"




events_t g_events = { .mutex = PTHREAD_MUTEX_INITIALIZER };




typedef struct test_case_t {
	const char * description;    /**< Tester-friendly description of test case. */
	const char * message;        /**< Text to be sent to cwdaemon server in the MESSAGE request. Does not include caret! */
	bool attach_caret;           /**< Messages in corner-case tests may not include caret. */
} test_case_t;




static test_case_t g_test_cases[] = {
	{ .description = "basic test: mixed characters and a caret",           .message = "2 crows, one stork?",  .attach_caret = true, },
	{ .description = "basic test: two words and a caret",                  .message = "Hello world!",         .attach_caret = true, },
	{ .description = "basic test: single character and a caret",           .message = "f",                    .attach_caret = true, },
	{ .description = "basic test: few characters and a caret",             .message = "Texas",                .attach_caret = true, },
	{ .description = "basic test: empty text and a caret",                 .message = "",                     .attach_caret = true, },
	{ .description = "basic test: mixed characters and a caret (again)",   .message = "when, now, right: ",   .attach_caret = true, },
};




static int test_setup(cwdaemon_server_t * server, client_t * client, morse_receiver_t ** morse_receiver);
static int test_teardown(cwdaemon_server_t * server, client_t * client, morse_receiver_t ** morse_receiver);
static int run_test_cases(test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver);
static int evaluate_events(const events_t * events, const test_case_t * test_case);




int basic_caret_test(void)
{
	bool failure = false;
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	cwdaemon_server_t server = { 0 };
	client_t client = { 0 };
	morse_receiver_t * morse_receiver = NULL;

	if (0 != test_setup(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at test setup %s\n", "");
		failure = true;
		goto cleanup;
	}

	if (0 != run_test_cases(g_test_cases, n_test_cases, &client, morse_receiver)) {
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




static int evaluate_events(const events_t * events, const test_case_t * test_case)
{
	/* Expectation 1: there should be 2 events:
	    - Receiving some reply over socket from cwdaemon server,
	    - Receiving some Morse code on cwdevice.
	*/
	{
		const int expected = 2;
		if (expected != events->event_idx) {
			test_log_err("Test: unexpected count of events: %d\n", events->event_idx);
			return -1;
		}
	}




	/*
	  Expectation 2: events in proper order.

	  I'm not 100% sure what the correct order should be in perfect
	  implementation of cwdaemon. In 0.12.0 it is "socket receive" first, and
	  then "morse receive" second, unless a message sent to server ends with
	  space.

	  TODO acerion 2024.01.26: Double check the corner case with space at the
	  end of message.
	*/
	const event_t * event_0 = NULL;
	const event_t * event_1 = NULL;
	int i = 0;
	if (events->events[i].event_type != event_type_client_socket_receive) {
		test_log_warn("Test: unexpected type of event %d: %d\n", i, events->events[i].event_type);
		//return -1; /* TODO acerion 2024.01.26: uncomment. */
	}
	event_0 = &events->events[i];
	i++;

	if (events->events[i].event_type != event_type_morse_receive) {
		test_log_warn("Test: unexpected type of event %d: %d\n", i, events->events[i].event_type);
		//return -1; /* TODO acerion 2024.01.26: uncomment. */
	}
	event_1 = &events->events[i];
	i++;

	if (event_0->event_type != event_type_client_socket_receive
	    && event_1->event_type != event_type_client_socket_receive) {
		test_log_err("Test: none of events is 'client socket receive' event: %d, %d\n", event_0->event_type, event_1->event_type);
		return -1;
	}
	if (event_0->event_type != event_type_morse_receive
	    && event_1->event_type != event_type_morse_receive) {
		test_log_err("Test: none of events is 'morse receive' event: %d, %d\n", event_0->event_type, event_1->event_type);
		return -1;
	}

	const event_t * socket_event = event_0->event_type == event_type_client_socket_receive ? event_0 : event_1;
	const event_t * morse_event = event_0->event_type == event_type_morse_receive ? event_0 : event_1;




	/* Expectation 3: cwdaemon client has received over socket a correct
	   reply. */
	char escaped_received_reply[64] = { 0 };
	escape_string(socket_event->u.socket_receive.string, escaped_received_reply, sizeof (escaped_received_reply));
	char expected_reply[128] = { 0 };
	const int n = snprintf(expected_reply, sizeof (expected_reply), "one %s%c%c", test_case->message, '\r', '\n');
	if (0 != memcmp(expected_reply, socket_event->u.socket_receive.string, n)) {
		test_log_err("Test: didn't detect [%s] in reply received over network: [%s]\n", test_case->message, escaped_received_reply);
		return -1;
	}
	test_log_info("Test: correctly found [%s] in reply received over network [%s]\n", test_case->message, escaped_received_reply);




	/* Expectation 4: cwdaemon was behaving correctly enough to key a proper
	   Morse message on cwdevice. */
	if (!morse_receive_text_is_correct(morse_event->u.morse_receive.string, test_case->message)) {
		test_log_err("Test: didn't detect [%s] in received Morse message: [%s]\n", test_case->message, morse_event->u.morse_receive.string);
		return -1;
	}
	test_log_info("Test: correctly found [%s] in received Morse message [%s]\n", test_case->message, morse_event->u.morse_receive.string);




	/* Expectation 4: "Morse receive" event and "socket receive" event are
	   close to each other.

	   TODO acerion 2024.01.26: the threshold is still 0.5 seconds. That's a
	   lot. Try to bring it down. Notice that the time diff may depend on
	   Morse code speed (wpm).
	*/
	struct timespec diff = { 0 };
	if (event_0->tstamp.tv_sec < event_1->tstamp.tv_sec
	    || (event_0->tstamp.tv_sec == event_1->tstamp.tv_sec && event_0->tstamp.tv_nsec < event_1->tstamp.tv_nsec)) {

		timespec_diff(&event_0->tstamp, &event_1->tstamp, &diff);
	} else {
		timespec_diff(&event_1->tstamp, &event_0->tstamp, &diff);
	}
	const int threshold = 500L * 1000 * 1000; /* [nanoseconds] */
	if (diff.tv_sec == 0 && diff.tv_nsec < threshold) {
		test_log_info("Test: 'Morse receive' event and 'socket receive' event aren't too far apart: %ld.%09ld seconds\n", diff.tv_sec, diff.tv_nsec);
	} else {
		test_log_err("Test: 'Morse receive' event and 'socket receive' event are too far apart: %ld.%09ld seconds\n", diff.tv_sec, diff.tv_nsec);
		return -1;
	}




	test_log_info("Test: evaluation of test events was successful %s\n", "");

	return 0;
}




static int test_setup(cwdaemon_server_t * server, client_t * client, morse_receiver_t ** morse_receiver)
{
	bool failure = false;

	int wpm = 10;
	/* Remember that some receive timeouts in tests were selected when the
	   wpm was hardcoded to 10 wpm. Picking values lower than 10 may lead to
	   overrunning the timeouts. */
	cwdaemon_random_uint(10, 15, (unsigned int *) &wpm);


	/* Prepare local test instance of cwdaemon server. */
	const cwdaemon_opts_t cwdaemon_opts = {
		.tone           = 640,
		.sound_system   = CW_AUDIO_SOUNDCARD,
		.nofork         = true,
		.cwdevice_name  = TEST_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
	};
	if (0 != server_start(&cwdaemon_opts, server)) {
		test_log_err("Test: failed to start cwdaemon %s\n", "");
		failure = true;
	}


	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.01.24: remove casting. */
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server %s\n", "");
		failure = true;
	}
	client_socket_receive_enable(client);
	if (0 != client_socket_receive_start(client)) {
		test_log_err("Test: failed to start socket receiver %s\n", "");
		failure = true;
	}


	morse_receiver_config_t morse_config = { .wpm = wpm };
	*morse_receiver = morse_receiver_ctor(&morse_config);
	if (NULL == *morse_receiver) {
		test_log_err("Test: failed to create Morse receiver %s\n", "");
		failure = true;
	}

	return failure ? -1 : 0;
}




static int test_teardown(cwdaemon_server_t * server, client_t * client, morse_receiver_t ** morse_receiver)
{
	bool failure = false;

	morse_receiver_dtor(morse_receiver);

	/* Terminate local test instance of cwdaemon server. */
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

	client_socket_receive_stop(client);
	client_disconnect(client);
	client_dtor(client);

	return failure ? -1 : 0;
}




static int run_test_cases(test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver)
{
	bool failure = false;

	for (size_t i = 0; i < n_test_cases; i++) {
		const test_case_t * test_case = &test_cases[i];

		test_log_newline(); /* Visual separator. */
		test_log_info("Test: starting test case %zu / %zu: [%s]\n", i + 1, n_test_cases, test_case->description);

		if (0 != morse_receiver_start(morse_receiver)) {
			test_log_err("Test: failed to start Morse receiver %s\n", "");
			failure = true;
			break;
		}

		/* Send the message to be played. */
		if (test_case->attach_caret) {
			client_send_request_va(client, CWDAEMON_REQUEST_MESSAGE, "one %s^", test_case->message);
		} else {
			client_send_request_va(client, CWDAEMON_REQUEST_MESSAGE, "one %s", test_case->message);
		}

		morse_receiver_wait(morse_receiver);

		events_print(&g_events); /* For debug only. */
		if (0 != evaluate_events(&g_events, test_case)) {
			test_log_err("Test: evaluation of events has failed for test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			break;
		}

		/* Clear stuff before running next test case. */
		events_clear(&g_events);

		test_log_info("Test: test case %zu / %zu has succeeded\n\n", i + 1, n_test_cases);
	}

	return failure ? -1 : 0;
}




