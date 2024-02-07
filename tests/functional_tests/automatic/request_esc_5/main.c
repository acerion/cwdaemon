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

   Test of EXIT request (and of test code that starts a test instance of
   cwdaemon).
*/




#define _DEFAULT_SOURCE




#include "config.h"

/* For kill() on FreeBSD 13.2 */
#include <signal.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <errno.h>

#include "tests/library/client.h"
#include "tests/library/cwdevice_observer.h"
#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/events.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/process.h"
#include "tests/library/random.h"
#include "tests/library/sleep.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"
#include "tests/library/time_utils.h"




events_t g_events = { .mutex = PTHREAD_MUTEX_INITIALIZER };




static child_exit_info_t g_child_exit_info;




typedef struct test_case_t {
	const char * description;    /**< Human-readable description of the test case. */
	bool send_message_request;   /**< Whether in this test case we should send MESSAGE request. */
	const char * full_message;   /**< Full text of message to be played by cwdaemon. */
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
	{ .description = "exiting a cwdaemon server that has just started",      .send_message_request = false                            },
	{ .description = "exiting a cwdaemon server that played some message",   .send_message_request = true,  .full_message = "paris"   },
};




static int evaluate_events(const events_t * events, const test_case_t * test_case);
static int testcase_setup(cwdaemon_server_t * server, client_t * client, morse_receiver_t ** morse_receiver, const test_case_t * test_case);
static int testcase_run(const test_case_t * test_case, cwdaemon_server_t * server, client_t * client, morse_receiver_t * morse_receiver, events_t * events);
static int testcase_teardown(const test_case_t * test_case, client_t * client, morse_receiver_t ** morse_receiver, events_t * events);




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




int main(void)
{
#if 0
	if (!test_env_is_usable(test_env_libcw_without_signals)) {
		test_log_err("Test: preconditions for test env are not met, exiting\n");
		exit(EXIT_FAILURE);
	}
#endif

	const uint32_t seed = cwdaemon_srandom(0);
	test_log_debug("Test: random seed: 0x%08x (%u)\n", seed, seed);

	signal(SIGCHLD, sighandler);


	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);

	for (size_t i = 0; i < n_test_cases; i++) {

		const test_case_t * test_case = &g_test_cases[i];

		test_log_newline(); /* Visual separator. */
		test_log_info("Test: starting test case %zu / %zu: %s\n", i + 1, n_test_cases, test_case->description);

		bool failure = false;
		cwdaemon_server_t server = { 0 };
		client_t client = { 0 };
		morse_receiver_t * morse_receiver = NULL;

		if (0 != testcase_setup(&server, &client, &morse_receiver, test_case)) {
			test_log_err("Test: failed at setting up of test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

		if (0 != testcase_run(test_case, &server, &client, morse_receiver, &g_events)) {
			test_log_err("Test: failed at execution of test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}


	cleanup:
		if (0 != testcase_teardown(test_case, &client, &morse_receiver, &g_events)) {
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
static int testcase_setup(cwdaemon_server_t * server, client_t * client, morse_receiver_t ** morse_receiver, const test_case_t * test_case)
{
	bool failure = false;

	int wpm = 10;
	/* Remember that some receive timeouts in tests were selected when the
	   wpm was hardcoded to 10 wpm. Picking values lower than 10 may lead to
	   overrunning the timeouts. */
	cwdaemon_random_uint(10, 15, (unsigned int *) &wpm);


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
	g_child_exit_info.pid = server->pid;


	if (0 != client_connect_to_server(client, server->ip_address, server->l4_port)) {
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		failure = true;
	}

	if (test_case->send_message_request) {
		const morse_receiver_config_t morse_config = { .wpm = wpm };
		*morse_receiver = morse_receiver_ctor(&morse_config);
		if (NULL == *morse_receiver) {
			test_log_err("Test: failed to create Morse receiver %s\n", "");
			failure = true;
		}
	}

	return failure ? -1 : 0;
}




static int testcase_run(const test_case_t * test_case, cwdaemon_server_t * server, client_t * client, morse_receiver_t * morse_receiver, events_t * events)
{
	if (test_case->send_message_request) {

		if (0 != morse_receiver_start(morse_receiver)) {
			test_log_err("Test: failed to start Morse receiver %s\n", "");
			return -1;
		}

		/* Send the message to be played. */
		client_send_request(client, CWDAEMON_REQUEST_MESSAGE, test_case->full_message);

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
		client_send_request(client, CWDAEMON_REQUEST_EXIT, "");
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
			events_insert_sigchld_event(events, &g_child_exit_info);
		} else {
			/* There was never a signal from child (at least not in
			   reasonable time. This will be recognized by
			   evaluate_events(). */
		}
	}



	events_sort(events);
	events_print(events);
	if (0 != evaluate_events(events, test_case)) {
		test_log_err("Test: evaluation of events has failed %s\n", "");
		return -1;
	}
	test_log_info("Test: evaluation of events was successful %s\n", "");


	return 0;
}




/**
   @brief Clean up resources used to execute single test case
*/
static int testcase_teardown(const test_case_t * test_case, client_t * client, morse_receiver_t ** morse_receiver, events_t * events)
{
	if (test_case->send_message_request) {
		morse_receiver_dtor(morse_receiver);
	}

	/* Close our socket to cwdaemon server. */
	client_disconnect(client);
	client_dtor(client);

	/* Clear stuff before running next test case. */
	events_clear(events);
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
static int evaluate_events(const events_t * events, const test_case_t * test_case)
{
	/* Expectation 1: there should be N events:
	    - Morse receive (if test_case::message is empty then we don't expect this event to occur),
	    - us sending EXIT request to cwdaemon server,
	    - cwdaemon cleanly exits which is signalled by SIGCHLD signal received by this test program.
	*/
	{
		const int expected = test_case->send_message_request ? 3 : 2;
		if (expected != events->event_idx) {
			test_log_err("Expectation 1: unexpected count of events: %d (expected %d)\n", events->event_idx, expected);
			return -1;
		}
	}
	test_log_info("Expectation 1: count of events is correct: %d\n", events->event_idx);




	/* Expectation 2: events in proper order. */
	const event_t * morse = NULL;
	const event_t * exit_request = NULL;
	const event_t * sigchld = NULL;
	int i = 0;

	if (test_case->send_message_request) {
		if (events->events[i].event_type != event_type_morse_receive) {
			test_log_err("Expectation 2: unexpected type of event %d: %d\n", i, events->events[i].event_type);
			return -1;
		}
		morse = &events->events[i];
		i++;
	}


	if (events->events[i].event_type != event_type_request_exit) {
		test_log_err("Expectation 2: unexpected type of event %d: %d\n", i, events->events[i].event_type);
		return -1;
	}
	exit_request = &events->events[i];
	i++;

	if (events->events[i].event_type != event_type_sigchld) {
		test_log_err("Expectation 2: unexpected type of event %d: %d\n", i, events->events[i].event_type);
		return -1;
	}
	sigchld = &events->events[i];
	i++;
	test_log_info("Expectation 2: types of events are correct %s\n", "");




	/* Expectation 3: cwdaemon keyed a proper Morse message on cwdevice. */
	if (test_case->send_message_request) {
		if (!morse_receive_text_is_correct(morse->u.morse_receive.string, test_case->full_message)) {
			test_log_err("Expectation 3: received Morse message [%s] doesn't match text from message request [%s]\n",
			             morse->u.morse_receive.string, test_case->full_message);
			return -1;
		}
		test_log_info("Expectation 3: received Morse message [%s] matches test from message request [%s] (ignoring the first character)\n",
		              morse->u.morse_receive.string, test_case->full_message);
	} else {
		test_log_info("Expectation 3: skipping verification of Morse message, because this test doesn't play Morse code %s\n", "");
	}




	/* Expectation 4: cwdaemon exited cleanly. */
	const int wstatus = sigchld->u.sigchld.wstatus;
	const bool clean_exit = WIFEXITED(wstatus) && 0 == WEXITSTATUS(wstatus);
	if (!clean_exit) {
		test_log_err("Expectation 4: cwdaemon server didn't exit cleanly, wstatus = %d\n", wstatus);
		return -1;
	}
	test_log_info("Expectation 4: exit status of cwdaemon server is correct (expecting 0 / EXIT_SUCCESS): %d\n", wstatus);




	/* Expectation 5: time span between request to exit and the actual exit
	   was short (definition of "short" is not precise). */
	struct timespec diff = { 0 };
	timespec_diff(&exit_request->tstamp, &sigchld->tstamp, &diff);
	if (diff.tv_sec >= 2) { /* TODO acerion 2024.01.01: make the comparison more precise. Compare against 1.5 second. */
		test_log_err("Expectation 5: duration of exit was longer than expected: %ld.%09ld [seconds]\n", diff.tv_sec, diff.tv_nsec);
		return -1;
	}
	test_log_info("Expectation 5: cwdaemon server exited in expected amount of time: %ld.%09ld [seconds]\n", diff.tv_sec, diff.tv_nsec);




	test_log_info("Test: Evaluation of test events was successful %s\n", "");

	return 0;
}

