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

   Test of EXIT request.

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




static child_exit_info_t g_child_exit_info;




typedef struct test_case_t {
	const char * description;    /**< Human-readable description of the test case. */
	bool send_message_request;   /**< Whether in this test case we should send MESSAGE request. */
	const char * full_message;   /**< Full text of message to be played by cwdaemon. */
	event_t expected_events[EVENTS_MAX]; /**< Events that we expect to happen in this test case. */
} test_case_t;




/*
  There are two basic test cases: when EXIT request is being sent to cwdaemon
  that has just started and didn't do anything else, and when EXIT request is
  being sent to cwdaemon that has handled some request.

  I could of course come up with more test cases in which cwdaemon did some
  complicated stuff before it was asked to handle EXIT request, but that
  would be duplicating the situations covered by other functional tests. In
  the other functional tests I plan to check how cwdaemon has handled the
  final EXIT request too. That should be enough to cover the more complicated
  situations.
*/
static test_case_t g_test_cases[] = {
	{ .description          = "exiting a cwdaemon server that has just started",
	  .send_message_request = false,
	  .expected_events      = { { .event_type = event_type_request_exit   },
	                            { .event_type = event_type_sigchld        }, },


	},

	{ .description          = "exiting a cwdaemon server that played some message",
	  .send_message_request = true,
	  .full_message         = "paris",
	  .expected_events      = { { .event_type = event_type_morse_receive  },
	                            { .event_type = event_type_request_exit   },
	                            { .event_type = event_type_sigchld        }, },
	},
};




static int evaluate_events(events_t * events, const test_case_t * test_case);
static int testcase_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_case_t * test_case, const test_options_t * test_opts);
static int testcase_run(const test_case_t * test_case, server_t * server, client_t * client, morse_receiver_t * morse_receiver, events_t * events);
static int testcase_teardown(const test_case_t * test_case, client_t * client, morse_receiver_t * morse_receiver);




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
#if 0
	if (!test_env_is_usable(test_env_libcw_without_signals)) {
		test_log_err("Test: preconditions for test env are not met, exiting\n");
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

	signal(SIGCHLD, sighandler);


	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);

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
			exit(EXIT_FAILURE);
		}
		test_log_info("Test: test case #%zu/%zu succeeded\n\n", i + 1, n_test_cases);
	}

	exit(EXIT_SUCCESS);
}




/**
   @brief Prepare resources used to execute single test case
*/
static int testcase_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_case_t * test_case, const test_options_t * test_opts)
{
	bool failure = false;

	const int wpm = test_get_test_wpm();

	const server_options_t server_opts = {
		.tone           = test_get_test_tone(),
		.sound_system   = test_opts->sound_system,
		.nofork         = true,
		.cwdevice_name  = TESTS_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
		.supervisor_id  = test_opts->supervisor_id,
	};

	if (0 != server_start(&server_opts, server)) {
		test_log_err("Test: failed to start cwdaemon %s\n", "");
		failure = true;
	}
	g_child_exit_info.pid = server->pid;


	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.03.22: remove casting. */
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		failure = true;
	}

	if (test_case->send_message_request) {
		const morse_receiver_config_t morse_config = { .wpm = wpm };
		if (0 != morse_receiver_ctor(&morse_config, morse_receiver)) {
			test_log_err("Test: failed to create Morse receiver %s\n", "");
			failure = true;
		}
	}

	return failure ? -1 : 0;
}




static int testcase_run(const test_case_t * test_case, server_t * server, client_t * client, morse_receiver_t * morse_receiver, events_t * events)
{
	if (test_case->send_message_request) {

		if (0 != morse_receiver_start(morse_receiver)) {
			test_log_err("Test: failed to start Morse receiver %s\n", "");
			return -1;
		}

		/* Send the message to be played. */
		client_send_message(client, test_case->full_message, strlen(test_case->full_message) + 1);

		morse_receiver_wait(morse_receiver);
	} else {
		/* Sending an EXIT request to a cwdaemon server that has just started
		   and did nothing else is also a valid case. */
	}




	/*
	  Main part of a test: test that EXIT request works.

	  Notice that the body of next block looks the same as implementation of
	  local_server_stop(). In this function we use the code explicitly
	  because we want to test EXIT request and we want to have it plainly
	  visible in the test code.
	*/

	{
		/* Enable this to get non-zero value of wstatus returned by waitpid()
		   for testing purposes. */
		// kill(server.pid, SIGKILL);

		/* First ask nicely for a clean exit. */
		client_send_esc_request(client, CWDAEMON_ESC_REQUEST_EXIT, "", 0);
		pthread_mutex_lock(&events->mutex);
		{
			clock_gettime(CLOCK_MONOTONIC, &events->events[events->event_idx].tstamp);
			events->events[events->event_idx].event_type = event_type_request_exit;
			events->event_idx++;
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

		if (0 != g_child_exit_info.sigchld_timestamp.tv_sec) {
			/*
			  SIGCHLD was received by test program at some point in time.
			  Record this in array of events.

			  Signal handler can record a timestamp, but can't add the event
			  to array of events itself. Let's do this here.

			  My tests show that there is no need to sort (by timestamp) the
			  array afterwards.
			*/
			events_insert_sigchld_event(server->events, &g_child_exit_info);
		} else {
			/* There was never a signal from child (at least not in
			   reasonable time. This will be recognized by
			   evaluate_events(). */
		}
	}


	return 0;
}




/**
   @brief Clean up resources used to execute single test case
*/
static int testcase_teardown(const test_case_t * test_case, client_t * client, morse_receiver_t * morse_receiver)
{
	/* We don't stop cwdaemon server here because the entire point of this
	   test is to stop the server in main part of the test :) */

	if (test_case->send_message_request) {
		morse_receiver_dtor(morse_receiver);
	}

	/* Close our socket to cwdaemon server. */
	client_disconnect(client);
	client_dtor(client);

	memset(&g_child_exit_info, 0, sizeof (g_child_exit_info));

	return 0;
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




	const int expected_events_cnt = events_get_count(test_case->expected_events);




	expectation_idx = 1;
	if (0 != expect_count_of_events(expectation_idx, events->event_idx, expected_events_cnt)) {
		return -1;
	}




	/* Expectation: correct types and order of events. */
	expectation_idx = 2;
	const event_t * morse_event = NULL;
	const event_t * exit_request = NULL;
	const event_t * sigchld_event = NULL;
	for (int i = 0; i < expected_events_cnt; i++) {
		if (test_case->expected_events[i].event_type != events->events[i].event_type) {
			test_log_err("Expectation %d: unexpected event %u at position %d\n", expectation_idx, events->events[i].event_type, i);
			return -1;
		}

		/* Get references to specific events in array of events. */
		switch (events->events[i].event_type) {
		case event_type_morse_receive:
			morse_event = &events->events[i];
			break;
		case event_type_sigchld:
			sigchld_event = &events->events[i];
			break;
		case event_type_request_exit:
			exit_request = &events->events[i];
			break;
		case event_type_none:
		case event_type_socket_receive:
		default:
			test_log_err("Expectation %d: unhandled event type %u at position %d\n", expectation_idx, events->events[i].event_type, i);
			return -1;
		}
	}
	if (NULL == sigchld_event) {
		/*  This test is to satisfy clang-tidy's check for NULL dereference. */
		test_log_err("Expectation %d: sigchld event was not found\n", expectation_idx);
		return -1;
	}

	test_log_info("Expectation %d: found expected types of events, in proper order\n", expectation_idx);




	expectation_idx = 3;
	if (test_case->send_message_request) {
		if (0 != expect_morse_receive_match(expectation_idx, morse_event->u.morse_receive.string, test_case->full_message)) {
			return -1;
		}
	} else {
		test_log_info("Expectation %d: skipping verification of Morse message, because this test doesn't play Morse code\n", expectation_idx);
	}




	/* Expectation: cwdaemon exited cleanly. */
	expectation_idx = 4;
	const int wstatus = sigchld_event->u.sigchld.wstatus;
	const bool clean_exit = WIFEXITED(wstatus) && 0 == WEXITSTATUS(wstatus);
	if (!clean_exit) {
		test_log_err("Expectation %d: cwdaemon server didn't exit cleanly, wstatus = %d\n", expectation_idx, wstatus);
		return -1;
	}
	test_log_info("Expectation %d: exit status of cwdaemon server is correct (expecting 0 / EXIT_SUCCESS): %d\n", expectation_idx, wstatus);




	/* Expectation: time span between request to exit and the actual exit
	   was short (definition of "short" is not precise). */
	expectation_idx = 5;
	struct timespec diff = { 0 };
	timespec_diff(&exit_request->tstamp, &sigchld_event->tstamp, &diff);
	if (diff.tv_sec >= 2) { /* TODO acerion 2024.01.01: make the comparison more precise. Compare against 1.5 second. */
		test_log_err("Expectation %d: duration of exit was longer than expected: %ld.%09ld [seconds]\n", expectation_idx, diff.tv_sec, diff.tv_nsec);
		return -1;
	}
	test_log_info("Expectation %d: cwdaemon server exited in expected amount of time: %ld.%09ld [seconds]\n", expectation_idx, diff.tv_sec, diff.tv_nsec);




	test_log_info("Test: Evaluation of test events was successful %s\n", "");

	return 0;
}

