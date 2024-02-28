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
   @file

   Test for proper re-registration of libcw keying callback when handling RESET
   request. https://github.com/acerion/cwdaemon/issues/6

   See also "#define CWDAEMON_GITHUB_ISSUE_6_FIXED" in src/cwdaemon.c.
*/




#define _DEFAULT_SOURCE




#include "config.h"

//#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <time.h>

#include "tests/library/client.h"
//#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/events.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/server.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"




static int evaluate_events(events_t * events, const char * message1, const char * message2);
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_run(client_t * client, morse_receiver_t * morse_receiver, const char * message1, const char * message2);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);




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
	events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
	server_t server = { .events = &events };
	client_t client = { .events = &events };
	morse_receiver_t morse_receiver = { .events = &events };
	const char * message1 = "paris";
	const char * message2 = "finger";


	if (0 != test_setup(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at setting up of test %s\n", "");
		failure = true;
		goto cleanup;
	}

	if (0 != test_run(&client, &morse_receiver, message1, message2)) {
		test_log_err("Test: failed at execution of test %s\n", "");
		failure = true;
		goto cleanup;
	}

	if (0 != evaluate_events(&events, message1, message2)) {
		test_log_err("Test: evaluation of events has failed %s\n", "");
		failure = true;
		goto cleanup;
	}

 cleanup:
	if (0 != test_teardown(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at tear-down for test %s\n", "");
		failure = true;
	}

	if (failure) {
		test_log_err("Test: test failed%s\n", "");
		exit(EXIT_FAILURE);
	}
	test_log_info("Test: test succeeded %s\n\n", "");

	exit(EXIT_SUCCESS);
}




/**
   @brief Prepare resources used to execute single test case
*/
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver)
{
	bool failure = false;

	int wpm = 10;
	/* Remember that some receive timeouts in tests were selected when the
	   wpm was hardcoded to 10 wpm. Picking values lower than 10 may lead to
	   overrunning the timeouts. */
	cwdaemon_random_uint(10, 15, (unsigned int *) &wpm);


	const cwdaemon_opts_t cwdaemon_opts = {
		.tone               = 630,
		.sound_system       = CW_AUDIO_SOUNDCARD,
		.nofork             = true,
		.cwdevice_name      = TEST_TTY_CWDEVICE_NAME,
		.wpm                = wpm,
	};
	if (0 != server_start(&cwdaemon_opts, server)) {
		test_log_err("Test: failed to start cwdaemon server %s\n", "");
		failure = true;
	}

	if (0 != client_connect_to_server(client, server->ip_address, server->l4_port)) {
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		failure = true;
	}

	const morse_receiver_config_t morse_config = { .wpm = wpm };
	if (0 != morse_receiver_ctor(&morse_config, morse_receiver)) {
		test_log_err("Test: failed to create Morse receiver %s\n", "");
		failure = true;
	}

	return failure ? -1 : 0;
}




static int test_run(client_t * client, morse_receiver_t * morse_receiver, const char * message1, const char * message2)
{
	/* This sends a text request to cwdaemon server that works in initial state,
	   i.e. reset command was not sent yet, so cwdaemon should not be
	   broken yet. */
	{
		if (0 != morse_receiver_start(morse_receiver)) {
			test_log_err("Test: failed to start Morse receiver (%d)\n", 1);
			return -1;
		}

		client_send_message(client, message1, strlen(message1) + 1);

		morse_receiver_wait(morse_receiver);
	}


	/* This would break the cwdaemon before a fix to
	   https://github.com/acerion/cwdaemon/issues/6 was applied. */
	client_send_esc_request(client, CWDAEMON_ESC_REQUEST_RESET, "", 0);


	/* This sends a text request to cwdaemon that works in "after reset"
	   state. A fixed cwdaemon should reset itself correctly. */
	{
		if (0 != morse_receiver_start(morse_receiver)) {
			test_log_err("Test: failed to start Morse receiver (%d)\n", 2);
			return -1;
		}

		client_send_message(client, message2, strlen(message2) + 1);

		morse_receiver_wait(morse_receiver);
	}

	return 0;
}




/**
   @brief Clean up resources used to execute single test case
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

	morse_receiver_dtor(morse_receiver);

	/*
	  Close our socket to cwdaemon server. cwdaemon may be stopped, but let's
	  still try to close socket on our end.
	*/
	client_disconnect(client);
	client_dtor(client);

	return failure ? -1 : 0;
}




static int evaluate_events(events_t * events, const char * message1, const char * message2)
{
	events_sort(events);
	events_print(events);


	/* Expectation 1: there are two events: Morse code was keyed (and received) on cwdevice twice. */
	if (2  != events->event_idx) {
		test_log_err("Expectation 1: incorrect count of events: %d\n", events->event_idx);
		return -1;
	}
	test_log_info("Expectation 1: correct count of test events: %d\n", events->event_idx);




	/* Expectation 2: both events are of type "Morse receive". */
	event_t * morse1 = &events->events[0];
	event_t * morse2 = &events->events[1];
	if (morse1->event_type != event_type_morse_receive
	    || morse2->event_type != event_type_morse_receive) {

		test_log_err("Expectation 2: incorrect type of event(s): %d, %d\n", morse1->event_type, morse2->event_type);
		return -1;
	}
	test_log_info("Expectation 2: correct types of test events: %d, %d\n", morse1->event_type, morse2->event_type);




	/* Expectation 3: both Morse receive events contain correct received text. */
	const char * received_string1 = morse1->u.morse_receive.string;
	const char * received_string2 = morse2->u.morse_receive.string;
	if (!morse_receive_text_is_correct(received_string1, message1)
	    || !morse_receive_text_is_correct(received_string2, message2)) {

		test_log_err("Expectation 3: incorrect text in Morse receive event(s): [%s], [%s]\n",
		             received_string1, received_string2);
		return -1;
	}
	test_log_info("Expectation 3: correct text in Morse receive events: [%s], [%s]\n",
	              received_string1, received_string2);




	test_log_info("Test: evaluation of test events was successful %s\n", "");

	return 0;
}

