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
//#include <time.h>
//#include <unistd.h>

#include <errno.h>

#include "src/cwdaemon.h"
#include "tests/library/client.h"
//#include "tests/library/cwdevice_observer.h"
//#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/events.h"
#include "tests/library/expectations.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/server.h"
//#include "tests/library/sleep.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"
#include "tests/library/test_options.h"
#include "tests/library/time_utils.h"




static child_exit_info_t g_child_exit_info;




typedef struct test_case_t {
	const char * description;  /** Human-readable description of the test case. */
	test_request_t full_message; /** Full text of message to be played by cwdaemon. */
	bool expected_fail;        /** Whether we expect cwdaemon to fail to start correctly due to invalid port number. */
	int port;                  /** Value of port passed to cwdaemon. */
	event_t expected_events[EVENTS_MAX];   /**< Events that we expect to happen in this test case. */
} test_case_t;




static test_case_t g_test_cases[] = {
	/*
	  port == -1 will be interpreted by code in server.c as "pass port 0 to cwdaemon".

	  TODO acerion 2024.03.28: Come up with a better representation of port
	  to avoid such special cases. Current solution is not clear.
	 */
	{ .description = "failure case: port 0",        .full_message = TESTS_SET_BYTES("paris"),  .expected_fail = true,   .port = -1,
	  .expected_events  = { { .etype = etype_sigchld }, }, },

	{ .description = "failure case: port 1",        .full_message = TESTS_SET_BYTES("paris"),  .expected_fail = true,   .port = 1,
	  .expected_events  = { { .etype = etype_sigchld }, }, },
	{ .description = "failure case: port MIN - 2",  .full_message = TESTS_SET_BYTES("paris"),  .expected_fail = true,   .port = CWDAEMON_NETWORK_PORT_MIN - 2,
	  .expected_events  = { { .etype = etype_sigchld }, }, },
	{ .description = "failure case: port MIN - 1",  .full_message = TESTS_SET_BYTES("paris"),  .expected_fail = true,   .port = CWDAEMON_NETWORK_PORT_MIN - 1,
	  .expected_events  = { { .etype = etype_sigchld }, }, },

	/* All valid ports between MIN and MAX are indirectly tested by other
	   functional tests that use random valid port. Here we just explicitly
	   test the MIN and MAX itself */
	{ .description = "success case: port MIN",      .full_message = TESTS_SET_BYTES("paris"),  .expected_fail = false,  .port = CWDAEMON_NETWORK_PORT_MIN,
	  .expected_events  = { { .etype = etype_morse }, }, },
	{ .description = "success case: port MAX",      .full_message = TESTS_SET_BYTES("paris"),  .expected_fail = false,  .port = CWDAEMON_NETWORK_PORT_MAX,
	  .expected_events  = { { .etype = etype_morse }, }, },

	{ .description = "failure case: port MAX + 1",  .full_message = TESTS_SET_BYTES("paris"),  .expected_fail = true,   .port = CWDAEMON_NETWORK_PORT_MAX + 1,
	  .expected_events  = { { .etype = etype_sigchld }, }, },
	{ .description = "failure case: port MAX + 2",  .full_message = TESTS_SET_BYTES("paris"),  .expected_fail = true,   .port = CWDAEMON_NETWORK_PORT_MAX + 2,
	  .expected_events  = { { .etype = etype_sigchld }, }, },
};




static int server_setup(server_t * server, const test_case_t * test_case, int * wpm, const test_options_t * test_opts);
static int testcase_setup(const server_t * server, client_t * client, morse_receiver_t * morse_receiver, int wpm);
static int testcase_run(const test_case_t * test_case, client_t * client, morse_receiver_t * morse_receiver);
static int testcase_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int evaluate_events(events_t * events, const test_case_t * test_case);




/*
  Since this test is starting a child process, we want to handle SIGCHLD
  signal.
*/
static void sighandler(int sig)
{
	if (SIGCHLD == sig) {
		g_child_exit_info.waitpid_retv = waitpid(g_child_exit_info.pid, &g_child_exit_info.wstatus, 0);
		clock_gettime(CLOCK_MONOTONIC, &g_child_exit_info.sigchld_timestamp);
	}
}




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
	test_log_debug("Test: random seed: 0x%08x (%u)\n", seed, seed);

	signal(SIGCHLD, sighandler);


	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);


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

		if (test_case->expected_fail) {
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
			exit(EXIT_FAILURE);
		}
		test_log_info("Test: test case #%zu/%zu succeeded\n\n", i + 1, n_test_cases);
	}

	exit(EXIT_SUCCESS);
}




static int save_child_exit_to_events(const child_exit_info_t * child_exit_info, events_t * events)
{
	if (0 != child_exit_info->sigchld_timestamp.tv_sec) {
		/*
		  SIGCHLD was received by test program at some point in time.
		  Record this in array of events.

		  Signal handler can record a timestamp, but can't add the event
		  to array of events itself. Let's do this here.

		  My tests show that there is no need to sort (by timestamp) the
		  array afterwards.
		*/
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
   test_case_t::expected_fail.

   @param[out] server cwdaemon server to set up
   @param[in] test_case Test case according to which to set up a server
   @param[out] wpm Morse code speed used by server
   @param[in/out] events Events containter

   @return 0 if starting of a server ended as expected
   @return -1 if starting of server ended not as expected
*/
static int server_setup(server_t * server, const test_case_t * test_case, int * wpm, const test_options_t * test_opts)
{
	*wpm = tests_get_test_wpm();

	const server_options_t server_opts = {
		.tone           = tests_get_test_tone(),
		.sound_system   = test_opts->sound_system,
		.nofork         = true,
		.cwdevice_name  = TESTS_TTY_CWDEVICE_NAME,
		.wpm            = *wpm,
		.l4_port        = test_case->port,
		.supervisor_id  = test_opts->supervisor_id,
	};
	const int retv = server_start(&server_opts, server);
	if (0 != retv) {
		save_child_exit_to_events(&g_child_exit_info, server->events);
		if (test_case->expected_fail) {
			return 0; /* Setting up of server has failed, as expected. */
		} else {
			test_log_err("Test: unexpected failure to start cwdaemon with valid port %d\n", test_case->port);
			return -1;
		}
	} else {
		if (test_case->expected_fail) {
			test_log_err("Test: unexpected success in starting cwdaemon with invalid port %d\n", test_case->port);
			return -1;
		} else {
			return 0; /* Setting up of server has succeeded, as expected. */
		}
	}
}




/**
   @brief Prepare resources used to execute single test case
*/
static int testcase_setup(const server_t * server, client_t * client, morse_receiver_t * morse_receiver, int wpm)
{
	bool failure = false;


	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.01.29: remove casting. */
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		failure = true;
	}


	const morse_receiver_config_t morse_config = { .wpm = wpm };
	if (0 != morse_receiver_configure(&morse_config, morse_receiver)) {
		test_log_err("Test: failed to configure Morse receiver %s\n", "");
		failure = true;
	}


	return failure ? -1 : 0;
}




static int testcase_run(const test_case_t * test_case, client_t * client, morse_receiver_t * morse_receiver)
{
	if (0 != morse_receiver_start(morse_receiver)) {
		test_log_err("Test: failed to start Morse receiver %s\n", "");
		return -1;
	}

	/* Send the message to be played to double-check that a cwdaemon server
	   is running, and that it's listening on a network socket on a port
	   specified in test case.. */
	client_send_request(client, &test_case->full_message);

	morse_receiver_wait_for_stop(morse_receiver);


	return 0;
}




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




static int evaluate_events(events_t * events, const test_case_t * test_case)
{
	events_sort(events);
	events_print(events);
	int expectation_idx = 0; /* To recognize failing expectations more easily. */




	const int expected_events_cnt = events_get_count(test_case->expected_events);




	expectation_idx = 1;
	if (0 != expect_count_of_events(expectation_idx, events->events_cnt, expected_events_cnt)) {
		return -1;
	}




	/* Expectation: correct types and order of events. */
	expectation_idx = 2;
	const event_t * morse_event = NULL;
	const event_t * sigchld_event = NULL;
	for (int i = 0; i < expected_events_cnt; i++) {
		if (test_case->expected_events[i].etype != events->events[i].etype) {
			test_log_err("Expectation %d: unexpected event %u at position %d\n", expectation_idx, events->events[i].etype, i);
			return -1;
		}

		/* Get references to specific events in array of events. */
		switch (events->events[i].etype) {
		case etype_morse:
			morse_event = &events->events[i];
			break;
		case etype_sigchld:
			sigchld_event = &events->events[i];
			break;
		case etype_none:
		case etype_reply:
		case etype_req_exit:
		default:
			test_log_err("Expectation %d: unhandled event type %u at position %d\n", expectation_idx, events->events[i].etype, i);
			return -1;
		}
	}
	if (test_case->expected_fail == true && NULL == sigchld_event) {
		/*  This test is to satisfy clang-tidy's check for NULL dereference. */
		test_log_err("Expectation %d: cwdaemon was expected to fail but sigchld event was not found\n", expectation_idx);
		return -1;
	}
	test_log_info("Expectation %d: found expected types of events, in proper order\n", expectation_idx);





	/* Expectation: when we use wrong port option, cwdaemon terminates in expected way. */
	expectation_idx = 3;
	if (test_case->expected_fail) {
		const int wstatus = sigchld_event->u.sigchld.wstatus;
		/* cwdaemon should have exited when it detected invalid value of port
		   option. */
		if (!WIFEXITED(wstatus)) {
			test_log_err("Expectation %d: failure case: cwdaemon did not exit, wstatus = 0x%04x\n", expectation_idx, (unsigned int) wstatus);
			return -1;
		}
		if (EXIT_FAILURE != WEXITSTATUS(wstatus)) {
			test_log_err("Expectation %d: failure case: incorrect exit status (expected %04x/EXIT_FAILURE): 0x%04x\n", expectation_idx, (unsigned int) EXIT_FAILURE, (unsigned int) WEXITSTATUS(wstatus));
			return -1;
		}
		test_log_info("Expectation %d: failure case: exit status is as expected (0x%04x)\n", expectation_idx, (unsigned int) wstatus);
	} else {
		test_log_info("Expectation %d: evaluation of exit status was skipped for correctly started cwdaemon\n", expectation_idx);
	}




	expectation_idx = 4;
	if (!test_case->expected_fail) {
		char expected[1024] = { 0 };
		snprintf(expected, test_case->full_message.n_bytes + 1, "%s", test_case->full_message.bytes);
		if (0 != expect_morse_match(expectation_idx, &morse_event->u.morse_receive, expected)) {
			return -1;
		}
	} else {
		test_log_info("Expectation %d: evaluation of Morse message was skipped for incorrectly started cwdaemon\n", expectation_idx);
	}




	test_log_info("Test: evaluation of test events was successful %s\n", "");

	return 0;
}

