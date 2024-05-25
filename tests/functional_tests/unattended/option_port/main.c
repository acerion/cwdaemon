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

   Test of special cases for "-p"/"--port" command line option.

   In general there are several areas that can be tested when it comes to
   specifying network port for cwdaemon process. Only the last one of them is
   tested here.

    - using short option ("-p") vs. long option ("--port"). This is already
      covered by test library's get_option_port() function that selects one
      of the two forms at random.

    - specifying any valid port number (from valid range). This is already
      done by other functional tests: cwdaemon process is started with a port
      number that is randomly selected from the valid range.

    - not passing any command line option for port, allowing cwdaemon run
      with its default port. This is already done also by test library's
      get_option_port() function (see "explicit port argument" in the
      function).

    - trying to start cwdaemon with unusual port numbers, e.g. 0, 1, 1023.
      This is done in this test.
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

#include <errno.h>

#include "src/cwdaemon.h"
#include "tests/library/client.h"
#include "tests/library/events.h"
#include "tests/library/expectations.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/server.h"
#include "tests/library/sleep.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"
#include "tests/library/test_options.h"
#include "tests/library/time_utils.h"




typedef struct test_case_t {
	char const * description;       ///< Human-readable description of the test case.
	test_request_t plain_request;   ///< PLAIN request to be played by cwdaemon.
	int port;                       ///< Value of port passed to cwdaemon.
	event_t expected[EVENTS_MAX];   ///< Events that we expect to happen in this test case.
} test_case_t;




static const char * g_test_name = "PORT option";
static child_exit_info_t g_child_exit_info;
static bool expected_fail(test_case_t const * test_case);




/// Whether we expect cwdaemon to fail to start correctly due to invalid port number.
static bool expected_fail(test_case_t const * test_case)
{
	return test_case->port < CWDAEMON_NETWORK_PORT_MIN || test_case->port > CWDAEMON_NETWORK_PORT_MAX;
}




/// @reviewed_on{2024.05.03}
static test_case_t g_test_cases[] = {
	/// port == -1 will be interpreted by code in server.c as "pass port 0 to
	/// cwdaemon".
	///
	/// TODO acerion 2024.03.28: Come up with a better representation of port
	/// to avoid such special cases. Current solution is not clear.
	{ .description = "failure case: port 0",
	  .port = -1,
	  .plain_request = TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_sigchld, .u.sigchld = { .exp_exited = true, .exp_exit_arg = EXIT_FAILURE, }, }, }, },

	{ .description = "failure case: port 1",
	  .port = 1,
	  .plain_request = TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_sigchld, .u.sigchld = { .exp_exited = true, .exp_exit_arg = EXIT_FAILURE, }, }, }, },

	{ .description = "failure case: port MIN - 2",
	  .port = CWDAEMON_NETWORK_PORT_MIN - 2,
	  .plain_request = TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_sigchld, .u.sigchld = { .exp_exited = true, .exp_exit_arg = EXIT_FAILURE, }, }, }, },

	{ .description = "failure case: port MIN - 1",
	  .port = CWDAEMON_NETWORK_PORT_MIN - 1,
	  .plain_request = TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_sigchld, .u.sigchld = { .exp_exited = true, .exp_exit_arg = EXIT_FAILURE, }, }, }, },




	/// All valid ports between MIN and MAX are indirectly tested by other
	/// functional tests that use random valid port. Below we just explicitly
	/// test the MIN and MAX itself.
	///
	/// sigchld event is not expected here because cwdaemon is stopped
	/// through EXIT request *AFTER* a test case is completed.
	{ .description = "success case: port MIN",
	  .port = CWDAEMON_NETWORK_PORT_MIN,
	  .plain_request =                                 TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris"), }, }, },

	{ .description = "success case: port MAX",
	  .port = CWDAEMON_NETWORK_PORT_MAX,
	  .plain_request =                                 TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris"), }, }, },




	{ .description = "failure case: port MAX + 1",
	  .port = CWDAEMON_NETWORK_PORT_MAX + 1,
	  .plain_request = TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_sigchld, .u.sigchld = { .exp_exited = true, .exp_exit_arg = EXIT_FAILURE, }, }, }, },

	{ .description = "failure case: port MAX + 2",
	  .port = CWDAEMON_NETWORK_PORT_MAX + 2,
	  .plain_request = TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_sigchld, .u.sigchld = { .exp_exited = true, .exp_exit_arg = EXIT_FAILURE, }, }, }, },
};




static int server_setup(server_t * server, const test_case_t * test_case, int * wpm, const test_options_t * test_opts);
static int testcase_setup(const server_t * server, client_t * client, morse_receiver_t * morse_receiver, int wpm);
static int testcase_run(const test_case_t * test_case, client_t * client, morse_receiver_t * morse_receiver);
static int testcase_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int evaluate_events(events_t const * events, test_case_t const * test_case);




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
		test_log_info("Test: starting test case %zu / %zu: [%s]\n", i + 1, n_test_cases, test_case->description);

		bool failure = false;
		events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
		server_t server = { .events = &events };
		client_t client = { .events = &events };
		morse_receiver_t morse_receiver = { .events = &events };


		int wpm = 0;
		if (0 != server_setup(&server, test_case, &wpm, &test_opts)) {
			test_log_err("Test: failed at setting up of server for test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

		if (expected_fail(test_case)) {
			/* We are expecting cwdaemon server to fail to start in
			   server_setup(). Without a server, calling testcase_setup() and
			   testcase_run() doesn't make sense. But still evaluate
			   collected events. */
			goto evaluate;
		}

		if (0 != testcase_setup(&server, &client, &morse_receiver, wpm)) {
			test_log_err("Test: failed at setting up of test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

		if (0 != testcase_run(test_case, &client, &morse_receiver)) {
			test_log_err("Test: running test case %zu / %zu has failed\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

	evaluate:
		events_sort(&events);
		if (0 != evaluate_events(&events, test_case)) {
			test_log_err("Test: evaluation of events has failed for test %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

	cleanup:
		if (0 != testcase_teardown(&server, &client, &morse_receiver)) {
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




/// @reviewed_on{2024.05.03}
static int save_child_exit_to_events(const child_exit_info_t * child_exit_info, events_t * events)
{
	if (0 != child_exit_info->sigchld_timestamp.tv_sec) {
		// SIGCHLD was received by test program at some point in time. Record
		// this in array of events.
		//
		// Signal handler can record a timestamp, but can't add the event to
		// non-global array of events itself. Let's do this here.
		events_insert_sigchld_event(events, child_exit_info);
	}

	return 0;
}




/**
   @brief Prepare cwdaemon server used to execute single test case

   Server is being prepared outside of testcase_setup() because in some cases
   we expect the server to fail. To properly handle "successful failure" in a
   given test case run, we need to separate setup of server (in this
   function) and setup of other resources.

   Again: it may be expected and desired that server fails to start, see
   expected_fail().

   @reviewed_on{2024.05.03}

   @param[out] server cwdaemon server to set up
   @param[in] test_case Test case according to which to set up a server
   @param[out] wpm Morse code speed to be used by server
   @param[in] test_opts Test's options from command line and from env

   @return 0 if starting of a server ended as expected
   @return -1 if starting of server ended not as expected
*/
static int server_setup(server_t * server, const test_case_t * test_case, int * wpm, const test_options_t * test_opts)
{
	*wpm = tests_get_test_wpm();

	const server_options_t server_opts = {
		.tone           = tests_get_test_tone(),
		.sound_system   = test_opts->sound_system,
		.cwdevice_name  = TESTS_TTY_CWDEVICE_NAME,
		.wpm            = *wpm,
		.l4_port        = test_case->port,
		.supervisor_id  = test_opts->supervisor_id,
	};
	const int retv = server_start(&server_opts, server);
	if (0 != retv) {

		// Give the signal handler for SIGCHLD some extra time to process the
		// SIGCHLD signal and update g_child_exit_info. Not 100% sure if it's
		// needed, but shouldn't hurt.
		test_millisleep_nonintr(100);

		save_child_exit_to_events(&g_child_exit_info, server->events);
		if (!expected_fail(test_case)) {
			test_log_err("Test: unexpected failure to start cwdaemon with valid port %d\n", test_case->port);
			return -1;
		}
		return 0; /* Setting up of server has failed, as expected. */
	} else {
		if (expected_fail(test_case)) {
			test_log_err("Test: unexpected success in starting cwdaemon with invalid port %d\n", test_case->port);
			return -1;
		}
		g_child_exit_info.pid = server->pid;
		return 0; /* Setting up of server has succeeded, as expected. */
	}
}




/**
   @brief Prepare resources used to execute single test case

   @reviewed_on{2024.05.03}
*/
static int testcase_setup(const server_t * server, client_t * client, morse_receiver_t * morse_receiver, int wpm)
{
	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.01.29: remove casting. */
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		return -1;
	}


	const morse_receiver_config_t morse_config = { .wpm = wpm };
	if (0 != morse_receiver_configure(&morse_config, morse_receiver)) {
		test_log_err("Test: failed to configure Morse receiver %s\n", "");
		return -1;
	}


	return 0;
}




/// @reviewed_on{2024.05.03}
static int testcase_run(const test_case_t * test_case, client_t * client, morse_receiver_t * morse_receiver)
{
	if (0 != morse_receiver_start(morse_receiver)) {
		test_log_err("Test: failed to start Morse receiver %s\n", "");
		return -1;
	}

	/// Send the message to be played to double-check if a cwdaemon server is
	/// running, and that it's listening on a network socket on a port
	/// specified in test case.
	///
	/// If server was not supposed to start due to invalid port, but it has
	/// been started and it accepts the request and keys it on cwdevice, the
	/// keying will be recorded in events array and recognized as unexpected
	/// during evaluation of events.
	client_send_request(client, &test_case->plain_request);

	// Receive events on cwdevice (Morse code on keying pin AND/OR
	// ptt events on ptt pin).
	morse_receiver_wait_for_stop(morse_receiver);

	return 0;
}




/// @reviewed_on{2024.05.03}
static int testcase_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver)
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

	morse_receiver_deconfigure(morse_receiver);

	/* Close our socket to cwdaemon server. */
	client_disconnect(client);
	client_dtor(client);

	memset(&g_child_exit_info, 0, sizeof (g_child_exit_info));

	return failure ? -1 : 0;
}




/// @reviewed_on{2024.05.03}
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


	test_log_info("Test: evaluation of test events was successful for test case [%s]\n", test_case->description);

	return 0;
}

