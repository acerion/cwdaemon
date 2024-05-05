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




/**
   @file

   Use several clients to send messages from different sources.

   See how cwdaemon handles message Y while message X is still being played.
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
#include "tests/library/sleep.h"
#include "tests/library/socket.h"
#include "tests/library/string_utils.h"
#include "tests/library/test_env.h"
#include "tests/library/test_options.h"
#include "tests/library/time_utils.h"




#define N_CLIENTS 5




typedef struct test_case_t {
	const char * description;            /** Human-readable description of the test. */
	bool caret;                          /**< Whether client should send caret request. If not, then client should send <ESC>h request. */

	/**
	   Full text of message to be played by cwdaemon.
	*/
	test_request_t play_request;


	/**
	   What is being sent to cwdaemon server as expected value of reply.
	   (without leading 'h').
	*/
	test_request_t esc_h_request;
} test_case_t;




static test_case_t g_test_cases[N_CLIENTS] = {
	{ .description    = "client 1 data",
	  .caret          = false,
	  .play_request   = TESTS_SET_BYTES("request_1 11111 11111 11111 111111111 111111111?"),
	  .esc_h_request  = TESTS_SET_BYTES("\033hreply_111 11111 11111 11111 111111111 111111111!"),
	},

	{ .description    = "client 2 data",
	  .caret          = true,
	  .play_request   = TESTS_SET_BYTES("caret_222 22222 22222 22222 222222222 22222222?^"),
	},

	{ .description    = "client 3 data",
	  .caret          = false,
	  .play_request   = TESTS_SET_BYTES("request_3 33333 33333 33333 333333333 333333333?"),
	  .esc_h_request  = TESTS_SET_BYTES("\033hreply_333 33333 33333 33333 333333333 333333333!"),

	},

	{ .description    = "client 4 data",
	  .caret          = true,
	  .play_request   = TESTS_SET_BYTES("caret_444 44444 44444 44444 444444444 44444444?^"),

	},

	{ .description    = "client 5 data",
	  .caret          = false,
	  .play_request   = TESTS_SET_BYTES("request_555555555555555555555555555555555555555?"),
	  .esc_h_request  = TESTS_SET_BYTES("\033hreply_55555555555555555555555555555555555555555!"),
	},
};




static int test_setup(server_t * server, client_t * clients, morse_receiver_t * morse_receiver);
static int test_teardown(server_t * server, client_t * clients, morse_receiver_t * morse_receiver);
static int test_run(test_case_t * test_cases, size_t n_test_cases, client_t * clients, morse_receiver_t * morse_receiver);
static int evaluate_events(events_t * events) __attribute__((unused)) ;




int main(int argc, char * const * argv)
{
	if (!testing_env_is_usable(testing_env_libcw_without_signals)) {
		test_log_err("Test: preconditions for testing env are not met, exiting %s\n", "");
		exit(EXIT_FAILURE);
	}

	test_options_t test_opts = { .sound_system = CW_AUDIO_SOUNDCARD };
	if (0 != test_options_get(argc, argv, &test_opts)) {
		test_log_err("Test: failed to process env variables and command line options %s\n", "");
		exit(EXIT_FAILURE);
	}
	if (test_opts.invoked_help) {
		/* Help text was printed as requested. Now exit. */
		exit(EXIT_SUCCESS);
	}

	const uint32_t seed = cwdaemon_srandom(test_opts.random_seed);
	test_log_info("Test: random seed: 0x%08x (%u)\n", seed, seed);

	bool failure = false;
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
	server_t server = { .events = &events };
	client_t clients[N_CLIENTS] = {
		{ .events = &events },
		{ .events = &events },
		{ .events = &events },
		{ .events = &events },
		{ .events = &events },
	};
	morse_receiver_t morse_receiver = { .events = &events };

	if (0 != test_setup(&server, clients, &morse_receiver)) {
		test_log_err("Test: failed at test setup %s\n", "");
		failure = true;
		goto cleanup;
	}

	if (test_run(g_test_cases, n_test_cases, clients, &morse_receiver)) {
		test_log_err("Test: failed at running test %s\n", "");
		failure = true;
		goto cleanup;
	}

 cleanup:
	if (0 != test_teardown(&server, clients, &morse_receiver)) {
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
   Right now the test is not at the stage, where automatic evaluation of
   events could be done.

   @return 0
*/
static int evaluate_events(__attribute__((unused)) events_t * events)
{
	return 0;
}




/**
   @brief Prepare resources used to execute set of test
*/
static int test_setup(server_t * server, client_t * clients, morse_receiver_t * morse_receiver)
{
	bool failure = false;

	const int wpm = tests_get_test_wpm();



#if 0
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
#endif
	server->l4_port = CWDAEMON_NETWORK_PORT_DEFAULT;
	snprintf(server->ip_address, sizeof (server->ip_address), "127.0.0.1");


	for (int i = 0; i < N_CLIENTS; i++) {
		if (0 != client_connect_to_server(&clients[i], server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.01.27: remove casting. */
			test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
			failure = true;
		}
		client_socket_receive_enable(&clients[i]);
		if (0 != client_socket_receive_start(&clients[i])) {
			test_log_err("Test: failed to start socket receiver %s\n", "");
			failure = true;
		}
	}


	const morse_receiver_config_t morse_config = { .wpm = wpm };
	if (0 != morse_receiver_configure(&morse_config, morse_receiver)) {
		test_log_err("Test: failed to configure Morse receiver %s\n", "");
		failure = true;
	}


	return failure ? -1 : 0;
}




/**
   @brief Clean up resources used to execute set of test
*/
static int test_teardown(server_t * server, client_t * clients, morse_receiver_t * morse_receiver)
{
	bool failure = false;

	/* Terminate local test instance of cwdaemon server. Always do it first
	   since the server is the main trigger of events in the test. */
	if (0 != local_server_stop(server, &clients[0])) {
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

	for (int i = 0; i < N_CLIENTS; i++) {
		client_socket_receive_stop(&clients[i]);
		client_disconnect(&clients[i]);
		client_dtor(&clients[i]);
	}

	return failure ? -1 : 0;
}




/**
   @brief Run several clients in parallel, make them send messages (almost) in parallel
*/
static int test_run(test_case_t * test_cases, size_t n_test_cases, client_t * clients, morse_receiver_t * morse_receiver)
{
	bool failure = false;

	test_log_newline(); /* Visual separator. */
	test_log_info("Test: starting test %s\n", "");

	/* This is the actual test. */
	{
		if (0 != morse_receiver_start(morse_receiver)) {
			test_log_err("Test: failed to start Morse receiver %s\n", "");
			failure = true;
		}


		const int n_iterations = 100000;
		for (int iter = 0; iter < n_iterations; iter++) {
			for (size_t c = 0; c < n_test_cases; c++) {
				test_case_t * test_case = &test_cases[c];

				/* Insert unique id into message to easily recognize it in cwademon's logs. */
				char id_buffer[16] = { 0 };
				snprintf(id_buffer, sizeof (id_buffer), ">%zu_%08d<", c, iter);
				const size_t id_len = strlen(id_buffer);
				memcpy(test_case->play_request.bytes + 10, id_buffer, id_len);

				if (test_case->caret) {
					/* Send the caret message to be played. */
					test_log_info("Test: client %zu: sending caret request [%s]\n", c, test_case->play_request.bytes);
					client_send_request(&clients[c], &test_case->play_request);
				} else {
					/* Ask cwdaemon to send us this reply back after playing a message. */
					client_send_request(&clients[c], &test_case->esc_h_request);

					/* Send the message to be played. */
					test_log_info("Test: client %zu: sending non-caret request [%s]\n", c, test_case->play_request.bytes);
					client_send_request(&clients[c], &test_case->play_request);
				}
				unsigned int delay = 0;

				/* For 1-based counter displays. */
				const int iter_1 = iter + 1;
				const float fiter_1 = (float) iter_1;
				const float n_fiterations = (float) n_iterations;

				//cwdaemon_random_uint(0, 5, &delay);
				test_log_info("Test: iteration %d / %d (%7.3f%%): sleeping for %u seconds before sending next request\n",
				              iter_1, n_iterations,
				              (double) ((fiter_1 / n_fiterations) * 100.0F),
				              delay);
				test_sleep_nonintr(delay);
			}
		}

		morse_receiver_wait_for_stop(morse_receiver);
	}

#if 0
	/* Validation of test run. */
	if (0 != evaluate_events(events, test_case)) {
		test_log_err("Test: evaluation of events has failed for test case %zu / %zu\n", i + 1, n_test_cases);
		failure = true;
	}
	/* Clear stuff before running next test case. */
	events_clear(events);
#endif


	test_log_info("Test: test ended %s\n\n", "");

	return failure ? -1 : 0;
}

