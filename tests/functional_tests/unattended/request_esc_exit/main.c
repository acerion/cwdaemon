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

   Test of EXIT Escape request.

   The test only tests exit of cwdaemon in two cases:
    - when cwdaemon was only started (without handling any request),
    - when cwdaemon handled MESSAGE request before being asked to handle EXIT
      request.

   Other functional tests (tests in other dirs) also send EXIT request at the
   end of test of at the end of test case. Those instances cover other
   situations, where cwdaemon is asked to handle EXIT request after doing
   misc actions, including handling different types of requests.
*/




#define _DEFAULT_SOURCE




#include "config.h"

/* For kill() on FreeBSD 13.2 */
#include <signal.h>
//#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
//#include <time.h>
//#include <unistd.h>

#include <errno.h>

#include "tests/library/client.h"
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
#include "tests/library/sleep.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"
#include "tests/library/test_options.h"
#include "tests/library/time_utils.h"




static char const * g_test_name = "EXIT Escape request";
static child_exit_info_t g_child_exit_info;




typedef struct test_case_t {
	char const * description;     /**< Human-readable description of the test case. */
	bool send_plain_request;      /**< Whether in this test case we should send PLAIN request. */
	test_request_t plain_request; /**< PLAIN request to be played by cwdaemon. */
	event_t expected[EVENTS_MAX]; /**< Events that we expect to happen in this test case. */
} test_case_t;




/*
  There are two basic test cases: when EXIT Escape request is being sent to
  cwdaemon that has just started and didn't do anything else, and when EXIT
  Escape request is being sent to cwdaemon that has already handled some
  other request.

  I could of course come up with more test cases in which cwdaemon did some
  complicated stuff before it was asked to handle EXIT Escape request, but
  that would be duplicating the situations covered by other functional tests.
  In the other functional tests I plan to check how cwdaemon has handled the
  final EXIT Escape request too. That should be enough to cover the more
  complicated situations. TODO (acerion) 2024.05.03: double-check that the "I
  plan to check" is really happening.
*/
/// @reviewed_on{2024.05.03}
static test_case_t g_test_cases[] = {
	{ .description = "exiting a cwdaemon server that has just started",
	  .send_plain_request = false,
	  .expected = { { .etype = etype_req_exit },
	                { .etype = etype_sigchld, .u.sigchld = { .exp_exited = true, .exp_exit_arg = EXIT_SUCCESS, }, }, },


	},

	{ .description = "exiting a cwdaemon server that already handled some request",
	  .send_plain_request = true,
	  .plain_request =                                 TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris"), },
	                { .etype = etype_req_exit },
	                { .etype = etype_sigchld, .u.sigchld = { .exp_exited = true, .exp_exit_arg = EXIT_SUCCESS, },  }, },
	},
};




static int evaluate_events(events_t const * recorded_events, test_case_t const * test_case);
static int testcase_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_case_t * test_case, const test_options_t * test_opts);
static int testcase_run(const test_case_t * test_case, server_t * server, client_t * client, morse_receiver_t * morse_receiver, events_t * events);
static int testcase_teardown(const test_case_t * test_case, client_t * client, morse_receiver_t * morse_receiver);




/// Since this test is observing exiting of a child process when the process
/// detects invalid command line options, we want to handle SIGCHLD signal.
///
/// @reviewed_on{2024.05.03}
static void sighandler(int sig)
{
	if (SIGCHLD == sig) {
		g_child_exit_info.waitpid_retv = waitpid(g_child_exit_info.pid, &g_child_exit_info.wstatus, 0);
		clock_gettime(CLOCK_MONOTONIC, &g_child_exit_info.sigchld_timestamp);
	}
}




/// @reviewed_on{2024.05.03}
int main(int argc, char * const * argv)
{
	if (!testing_env_is_usable(testing_env_libcw_without_signals
	                           | testing_env_real_cwdevice_is_present)) {
		test_log_err("Test: preconditions for testing env are not met, exiting test [%s]\n", g_test_name);
		exit(EXIT_FAILURE);
	}

	test_options_t test_opts = { .sound_system = CW_AUDIO_SOUNDCARD };
	if (0 != test_options_get(argc, argv, &test_opts)) {
		test_log_err("Test: failed to process env variables and command line options for test [%s]\n", g_test_name);
		exit(EXIT_FAILURE);
	}
	if (test_opts.invoked_help) {
		/* Help text was printed as requested. Now exit. */
		exit(EXIT_SUCCESS);
	}

	const uint32_t seed = cwdaemon_srandom(test_opts.random_seed);
	test_log_info("Test: random seed: 0x%08x (%u)\n", seed, seed);

	signal(SIGCHLD, sighandler);


	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);


	bool overall_test_failure = false; // Overall status of test.
	for (size_t i = 0; i < n_test_cases; i++) {

		const test_case_t * test_case = &g_test_cases[i];

		test_log_newline(); /* Visual separator. */
		test_log_info("Test: starting test case %zu / %zu: %s\n", i + 1, n_test_cases, test_case->description);

		bool failure = false;
		events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
		server_t server = { .events = &events };
		client_t client = { .events = &events };
		morse_receiver_t morse_receiver = { .events = &events };

		if (0 != testcase_setup(&server, &client, &morse_receiver, test_case, &test_opts)) {
			test_log_err("Test: failed at setting up of test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

		if (0 != testcase_run(test_case, &server, &client, &morse_receiver, &events)) {
			test_log_err("Test: failed at execution of test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

		events_sort(&events);
		if (0 != evaluate_events(&events, test_case)) {
			test_log_err("Test: evaluation of events has failed for test %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

	cleanup:
		if (0 != testcase_teardown(test_case, &client, &morse_receiver)) {
			test_log_err("Test: failed at tear-down for test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
		}

		if (failure) {
			test_log_err("Test: test case #%zu/%zu failed, terminating\n", i + 1, n_test_cases);
			overall_test_failure = true;
			break;
		}
		test_log_info("Test: test case #%zu/%zu succeeded\n\n", i + 1, n_test_cases);
	}

	if (overall_test_failure) {
		test_log_err("Test: FAIL ([%s] test)\n", g_test_name);
		test_log_newline(); /* Visual separator. */
		exit(EXIT_FAILURE);
	}
	test_log_info("Test: PASS ([%s] test)\n", g_test_name);
	test_log_newline(); /* Visual separator. */
	exit(EXIT_SUCCESS);
}




/**
   @brief Prepare resources used to execute single test case

   @reviewed_on{2024.05.03}
*/
static int testcase_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_case_t * test_case, const test_options_t * test_opts)
{
	const int wpm = tests_get_test_wpm();

	const server_options_t server_opts = {
		.tone           = tests_get_test_tone(),
		.sound_system   = test_opts->sound_system,
		.cwdevice_name  = TESTS_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
		.supervisor_id  = test_opts->supervisor_id,
	};
	if (0 != server_start(&server_opts, server)) {
		test_log_err("Test: failed to start cwdaemon for test case [%s]\n", test_case->description);
		return -1;
	}
	g_child_exit_info.pid = server->pid;


	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.03.22: remove casting. */
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		return -1;
	}

	if (test_case->send_plain_request) {
		const morse_receiver_config_t morse_config = { .wpm = wpm };
		if (0 != morse_receiver_configure(&morse_config, morse_receiver)) {
			test_log_err("Test: failed to configure Morse receiver %s\n", "");
			return -1;
		}
	}

	return 0;
}




/// @reviewed_on{2024.05.03}
static int testcase_run(const test_case_t * test_case, server_t * server, client_t * client, morse_receiver_t * morse_receiver, events_t * events)
{
	if (test_case->send_plain_request) {

		if (0 != morse_receiver_start(morse_receiver)) {
			test_log_err("Test: failed to start Morse receiver %s\n", "");
			return -1;
		}

		/* Send the PLAIN request to be played. */
		client_send_request(client, &test_case->plain_request);

		// Receive events on cwdevice (Morse code on keying pin AND/OR
		// ptt events on ptt pin).
		morse_receiver_wait_for_stop(morse_receiver);
	} else {
		// Don't send a plain request. Sending an EXIT request to a cwdaemon
		// server that has just started and did nothing else is also a valid
		// use case.
	}




	/*
	  Main part of a test: test that EXIT request works.

	  Notice that the body of next block looks the same as implementation of
	  local_server_stop(). In this function we use the code explicitly
	  because we want to test EXIT request and we want to have it plainly
	  visible in the test code.

	  TODO (acerion) 2024.05.03: double-check if we really want the
	  duplication (and to what degree) or not.
	*/

	{
		/* Enable this to get non-zero value of wstatus returned by waitpid()
		   for testing purposes. */
		// kill(server.pid, SIGKILL);

		/* First ask nicely for a clean exit. */
		client_send_esc_request(client, CWDAEMON_ESC_REQUEST_EXIT, "", 0);
		pthread_mutex_lock(&events->mutex);
		{
			// TODO (acerion) 2024.04.18: add checking if events_cnt is not
			// out of bounds.
			clock_gettime(CLOCK_MONOTONIC, &events->events[events->events_cnt].tstamp);
			events->events[events->events_cnt].etype = etype_req_exit;
			events->events_cnt++;
		}
		pthread_mutex_unlock(&events->mutex);

		/* Give cwdaemon some time to exit cleanly. cwdaemon needs ~1.3
		   second. */
		const int sleep_retv = test_sleep_nonintr(2);
		if (sleep_retv) {
			test_log_err("Test: error during sleep in cleanup %s\n", "");
		}

		/* Now check if test instance of cwdaemon server has disappeared as expected. */
		if (0 == kill(server->pid, 0)) {
			/* Process still exists, kill it. */
			test_log_err("Test: local test instance of cwdaemon process is still active despite being asked to exit, sending SIGKILL %s\n", "");
			/* The fact that we need to kill cwdaemon with a
			   signal is a bug. */
			kill(server->pid, SIGKILL);
			test_log_err("Test: local test instance of cwdaemon was forcibly killed %s\n", "");
			return -1;
		}

		// Give the signal handler for SIGCHLD some extra time to process the
		// SIGCHLD signal and update g_child_exit_info. Not 100% sure if it's
		// needed, but shouldn't hurt.
		test_millisleep_nonintr(100);

		if (0 != g_child_exit_info.sigchld_timestamp.tv_sec) {
			/// SIGCHLD was received by test program at some point in time.
			/// Record this in array of events.
			///
			/// Signal handler can record a timestamp, but can't add the
			/// event to global array of events itself. Let's do this here.
			events_insert_sigchld_event(server->events, &g_child_exit_info);
		} else {
			/// There was never a signal from child (at least not in
			/// reasonable time. This will be recognized by
			/// evaluate_events().
		}
	}


	return 0;
}




/**
   @brief Clean up resources used to execute single test case

   @reviewed_on{2024.05.03}
*/
static int testcase_teardown(const test_case_t * test_case, client_t * client, morse_receiver_t * morse_receiver)
{
	/* We don't stop cwdaemon server here because the entire point of this
	   test is to stop the server in main part of the testcase, before a
	   teardown of testcase is requested :) */

	if (test_case->send_plain_request) {
		morse_receiver_deconfigure(morse_receiver);
	}

	/* Close our socket to cwdaemon server. */
	client_disconnect(client);
	client_dtor(client);

	memset(&g_child_exit_info, 0, sizeof (g_child_exit_info));

	return 0;
}




/**
   @brief Evaluate events that were recorded during execution of single test
   case

   Look at contents of @p recorded_events and check if order and types of
   events are as expected.

   The events may include
     - receiving Morse code
     - receiving reply from cwdaemon server,
     - changes of state of PTT pin,
     - exiting of local instance of cwdaemon server process,

   @reviewed_on{2024.05.03}

   @return 0 if events are in proper order and of proper type
   @return -1 otherwise
*/
static int evaluate_events(events_t const * recorded_events, test_case_t const * test_case)
{
	events_print(recorded_events); // For debug only.


	int expectation_idx = 0; // To recognize failing expectations more easily.
	event_t const * const expected = test_case->expected;
	event_t const * const recorded = recorded_events->events;


	// Expectation: correct count, types, order and contents of events.
	expectation_idx = 1;
	if (0 != expect_count_type_order_contents(expectation_idx, expected, recorded)) {
		return -1;
	}

	// Expectation: server exits soon after receiving EXIT request.
	expectation_idx = 2;
	if (0 != expect_exit_and_sigchld_events_distance(expectation_idx, recorded)) {
		return -1;
	}


	test_log_info("Test: evaluation of test events was successful for test case [%s]\n", test_case->description);
	return 0;
}

