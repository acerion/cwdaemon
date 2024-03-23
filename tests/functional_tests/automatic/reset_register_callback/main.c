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
#include "tests/library/expectations.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/server.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"
#include "tests/library/test_options.h"




typedef struct test_case_t {
	event_t expected_events[EVENTS_MAX];
} test_case_t;




static test_case_t g_test_cases[] = {
	{ .expected_events = { { .event_type = event_type_morse_receive, },
	                       { .event_type = event_type_morse_receive, }, },
	}
};




static int evaluate_events(events_t * events, const test_case_t * test_case, const char * message1, const char * message2);
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_options_t * test_opts);
static int test_run(client_t * client, morse_receiver_t * morse_receiver, const char * message1, const char * message2);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);




int main(int argc, char * const * argv)
{
#if 0
	if (!test_env_is_usable(test_env_libcw_without_signals)) {
		test_log_err("Test: preconditions for test env are not met, exiting %s\n", "");
		exit(EXIT_FAILURE);
	}
#endif

	test_options_t test_opts = { .sound_system = CW_AUDIO_SOUNDCARD };
	if (0 != test_options_get(argc, argv, &test_opts)) {
		test_log_err("Test: failed to process command line options %s\n", "");
		exit(EXIT_FAILURE);
	}
	if (test_opts.invoked_help) {
		/* Help text was printed as requested. Now exit. */
		exit(EXIT_SUCCESS);
	}

	const uint32_t seed = cwdaemon_srandom(test_opts.random_seed);
	test_log_debug("Test: random seed: 0x%08x (%u)\n", seed, seed);

	bool failure = false;
	events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
	server_t server = { .events = &events };
	client_t client = { .events = &events };
	morse_receiver_t morse_receiver = { .events = &events };
	const char * message1 = "paris";
	const char * message2 = "finger";


	if (0 != test_setup(&server, &client, &morse_receiver, &test_opts)) {
		test_log_err("Test: failed at setting up of test %s\n", "");
		failure = true;
		goto cleanup;
	}

	if (0 != test_run(&client, &morse_receiver, message1, message2)) {
		test_log_err("Test: failed at execution of test %s\n", "");
		failure = true;
		goto cleanup;
	}

	if (0 != evaluate_events(&events, &g_test_cases[0], message1, message2)) {
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
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_options_t * test_opts)
{
	bool failure = false;

	const int wpm = test_get_test_wpm();

	const server_options_t server_opts = {
		.tone               = test_get_test_tone(),
		.sound_system       = test_opts->sound_system,
		.nofork             = true,
		.cwdevice_name      = TEST_TTY_CWDEVICE_NAME,
		.wpm                = wpm,
		.supervisor_id      = test_opts->supervisor_id,
	};
	if (0 != server_start(&server_opts, server)) {
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




static int evaluate_events(events_t * events, const test_case_t * test_case, const char * message1, const char * message2)
{
	events_sort(events);
	events_print(events);
	int expectation_idx = 0; /* To recognize failing expectations more easily. */




	const int expected_events_cnt = events_get_count(test_case->expected_events);




	expectation_idx = 1;
	if (0 != expect_count_of_events(expectation_idx, events->event_idx, expected_events_cnt)) {
		return -1;
	}





	/* Expectation: correct types of events. */
	expectation_idx = 2;
	for (int i = 0; i < expected_events_cnt; i++) {
		if (test_case->expected_events[i].event_type != events->events[i].event_type) {
			test_log_err("Expectation %d: unexpected event %d at position %d\n", expectation_idx, events->events[i].event_type, i);
			return -1;
		}
	}
	test_log_info("Expectation %d: found expected types of events\n", expectation_idx);




	/* Get references to specific events in array of events. */
	event_t * morse1 = &events->events[0];
	event_t * morse2 = &events->events[1];




	expectation_idx = 3;
	if (0 != expect_morse_receive_match(expectation_idx, morse1->u.morse_receive.string, message1)) {
		return -1;
	}




	expectation_idx = 4;
	if (0 != expect_morse_receive_match(expectation_idx, morse2->u.morse_receive.string, message2)) {
		return -1;
	}




	test_log_info("Test: evaluation of test events was successful %s\n", "");

	return 0;
}

