/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2023 Kamil Ignacak <acerion@wp.pl>
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




#include "src/lib/random.h"
#include "src/lib/sleep.h"
#include "tests/library/cwdevice_observer.h"
#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/events.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/process.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"
#include "tests/library/thread.h"
#include "tests/library/time_utils.h"




events_t g_events = { .mutex = PTHREAD_MUTEX_INITIALIZER };




/** Data type used in handling exit of a child process. */
typedef struct child_exit_info_t {
	pid_t pid;                            /**< pid of process on which to do waitpid(). */
	struct timespec sigchld_timestamp;    /**< timestamp at which sigchld has occurred. */
	int wstatus;                          /**< Second arg to waitpid(). */
	pid_t waitpid_retv;                   /**< Value returned by waitpid(). */
} child_exit_info_t;

static child_exit_info_t g_child_exit_info;




typedef struct test_case_t {
	const char * description;    /**< Tester-friendly description of test case. */
	bool send_message_request;   /**< Whether in this test case we should send MESSAGE request. */
	const char * message;        /**< Text to be sent to cwdaemon server in the MESSAGE request. */
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
	{ .description = "cwdaemon that has just started", .send_message_request = false                     },
	{ .description = "cwdaemon that played message",   .send_message_request = true,  .message = "msg"   },
};




static int events_evaluate(const events_t * events, const test_case_t * test_case);
static int run_test_case(const test_case_t * test_case);




static void print_waitpid(const char * prefix, int wstatus, pid_t pid, int err_no)
{
	if (pid > 0) {
		fprintf(stderr, "%s: Child process %d changed state\n", prefix, pid);
		if (WIFEXITED(wstatus)) {
			fprintf(stderr, "%s: Child process %d exited, exit status = %d\n", prefix, pid, WEXITSTATUS(wstatus));
		} else if (WIFSIGNALED(wstatus)) {
			fprintf(stderr, "%s: Child process %d was terminated by signal %d\n", prefix, pid, WTERMSIG(wstatus));
		} else if (WIFSTOPPED(wstatus)) {
			fprintf(stderr, "%s: Child process %d was stopped by signal %d\n", prefix, pid, WSTOPSIG(wstatus));
		} else {
			fprintf(stderr, "%s: Child process %d has unhandled wstatus %d / 0x%x\n", prefix, pid, wstatus, wstatus);
		}
	} else if (0 == pid) {
		fprintf(stderr, "%s: waitpid() returns zero\n", prefix);
	} else {
		fprintf(stderr, "%s: waitpid() returns %d, errno = %s\n", prefix, pid, strerror(err_no));
	}
}




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
		fprintf(stderr, "[EE] Preconditions for test env are not met, exiting\n");
		exit(EXIT_FAILURE);
	}
#endif

	const uint32_t seed = cwdaemon_srandom(0);
	fprintf(stderr, "[DD] Random seed: 0x%08x (%u)\n", seed, seed);

	signal(SIGCHLD, sighandler);


	const size_t n = sizeof (g_test_cases) / sizeof (g_test_cases[0]);

	bool failure = false;
	for (size_t i = 0; i < n; i++) {

		const test_case_t * test_case = &g_test_cases[i];

		test_log_newline(); /* Visual separator. */
		test_log_info("Starting test case %zu / %zu: %s\n", i + 1, n, test_case->description);

		if (0 != run_test_case(test_case)) {
			test_log_err("Running test case %zu / %zu has failed\n", i + 1, n);
			failure = true;
			break;
		}

		events_print(&g_events); /* For debug only. */
		if (0 != events_evaluate(&g_events, test_case)) {
			test_log_err("Evaluation of events has failed for test case %zu / %zu\n", i + 1, n);
			failure = true;
			break;
		}

		/* Clear stuff before running next test case. */
		events_clear(&g_events);
		memset(&g_child_exit_info, 0, sizeof (g_child_exit_info));

		test_log_info("Test case %zu / %zu has succeeded\n\n", i + 1, n);
	}

	if (failure) {
		exit(EXIT_FAILURE);
	} else {
		exit(EXIT_SUCCESS);
	}
}




static int run_test_case(const test_case_t * test_case)
{
	bool failure = false;

	int wpm = 10;
	/* Remember that some receive timeouts in tests were selected when the
	   wpm was hardcoded to 10 wpm. Picking values lower than 10 may lead to
	   overrunning the timeouts. */
	cwdaemon_random_uint(10, 15, (unsigned int *) &wpm);

	morse_receiver_config_t morse_config = { .wpm = wpm };
	thread_t morse_receiver_thread  = { .name = "Morse receiver thread", .thread_fn = morse_receiver_thread_fn, .thread_fn_arg = &morse_config };

	cwdaemon_server_t server = { 0 };
	client_t client = { 0 };
	const cwdaemon_opts_t cwdaemon_opts = {
		.tone           = 640,
		.sound_system   = CW_AUDIO_SOUNDCARD,
		.nofork         = true,
		.cwdevice_name  = TEST_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
	};

	if (0 != cwdaemon_start_and_connect(&cwdaemon_opts, &server, &client)) {
		fprintf(stderr, "[EE] Failed to start cwdaemon, exiting\n");
		failure = true;
		goto cleanup;
	}
	g_child_exit_info.pid = server.pid;




	if (test_case->send_message_request) {

		/* Send some regular message to cwdaemon server to make it change its internal state. */
		if (0 != thread_start(&morse_receiver_thread)) {
			fprintf(stderr, "[EE] Failed to start Morse receiver thread\n");
			failure = true;
			goto cleanup;
		}

		/* Send the message to be played. */
		client_send_request_va(&client, CWDAEMON_REQUEST_MESSAGE, "one %s", test_case->message);

		thread_join(&morse_receiver_thread);
		thread_cleanup(&morse_receiver_thread);
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
		client_send_request(&client, CWDAEMON_REQUEST_EXIT, "");
		pthread_mutex_lock(&g_events.mutex);
		{
			clock_gettime(CLOCK_MONOTONIC, &g_events.events[g_events.event_idx].tstamp);
			g_events.events[g_events.event_idx].event_type = event_type_request_exit;
			g_events.event_idx++;
		}
		pthread_mutex_unlock(&g_events.mutex);

		/* Give cwdaemon some time to exit cleanly. cwdaemon needs ~1.3
		   second. */
		const int sleep_retv = sleep_nonintr(2);
		if (sleep_retv) {
			fprintf(stderr, "[ERROR] error during sleep in cleanup\n");
		}

		/* Now check if test instance of cwdaemon server has disappeared as expected. */
		if (0 == kill(server.pid, 0)) {
			/* Process still exists, kill it. */
			fprintf(stderr, "[ERROR] Local test instance of cwdaemon process is still active despite being asked to exit, sending SIGKILL\n");
			/* The fact that we need to kill cwdaemon with a
			   signal is a bug. */
			kill(server.pid, SIGKILL);
			fprintf(stderr, "[ERROR] Local test instance of cwdaemon was forcibly killed\n");
			failure = true;
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
			pthread_mutex_lock(&g_events.mutex);
			{
				g_events.events[g_events.event_idx].tstamp = g_child_exit_info.sigchld_timestamp;
				g_events.events[g_events.event_idx].event_type = event_type_sigchld;
				g_events.events[g_events.event_idx].u.sigchld.wstatus = g_child_exit_info.wstatus;
				g_events.event_idx++;
				//qsort(g_events.events, g_events.event_idx, sizeof (event_t), event_sort_fn);
			}
			pthread_mutex_unlock(&g_events.mutex);
		} else {
			/* There was never a signal from child (at least not in
			   reasonable time. This will be recognized by
			   events_evaluate(). */
		}
	}



 cleanup:

	/*
	  Close our socket to cwdaemon server. cwdaemon may be stopped, but let's
	  still try to close socket on our end.
	*/
	client_disconnect(&client);

	return failure ? -1 : 0;
}




static int events_evaluate(const events_t * events, const test_case_t * test_case)
{
	/* Expectation 1: there should be N events:
	    - Morse receive (if test_case::message is empty then we don't expect this event to occur),
	    - us sending EXIT request to cwdaemon server,
	    - cwdaemon cleanly exits which is signalled by SIGCHLD signal received by this test program.
	*/
	{
		const int expected = test_case->send_message_request ? 3 : 2;
		if (expected != events->event_idx) {
			test_log_err("Unexpected count of events: %d\n", events->event_idx);
			return -1;
		}
	}




	/* Expectation 2: events in proper order. */
	const event_t * morse = NULL;
	const event_t * exit_request = NULL;
	const event_t * sigchld = NULL;
	int i = 0;

	if (test_case->send_message_request) {
		if (events->events[i].event_type != event_type_morse_receive) {
			test_log_err("Unexpected type of event %d: %d\n", i, events->events[i].event_type);
			return -1;
		}
		morse = &events->events[i];
		i++;
	}


	if (events->events[i].event_type != event_type_request_exit) {
		test_log_err("Unexpected type of event %d: %d\n", i, events->events[i].event_type);
		return -1;
	}
	exit_request = &events->events[i];
	i++;

	if (events->events[i].event_type != event_type_sigchld) {
		test_log_err("Unexpected type of event %d: %d\n", i, events->events[i].event_type);
		return -1;
	}
	sigchld = &events->events[i];
	i++;




	/* Expectation 3: cwdaemon was behaving correctly enough to key a proper
	   Morse message on cwdevice. */
	if (test_case->send_message_request) {
		if (!morse_receive_text_is_correct(morse->u.morse_receive.string, test_case->message)) {
			test_log_err("Didn't detect [%s] in received Morse message: [%s]\n", test_case->message, morse->u.morse_receive.string);
			return -1;
		}
		test_log_info("Correctly found [%s] in received Morse message [%s]\n", test_case->message, morse->u.morse_receive.string);
	}




	/* Expectation 4: cwdaemon exited cleanly. */
	const int wstatus = sigchld->u.sigchld.wstatus;
	const bool clean_exit = WIFEXITED(wstatus) && 0 == WEXITSTATUS(wstatus);
	if (!clean_exit) {
		test_log_err("cwdaemon server didn't exit cleanly, wstatus = %d\n", wstatus);
		return -1;
	}
	test_log_info("Exit status of cwdaemon server is correct (expecting 0): %d\n", wstatus);




	/* Expectation 5: time span between request to exit and the actual exit
	   was short (definition of "short" is not precise). */
	struct timespec diff = { 0 };
	timespec_diff(&exit_request->tstamp, &sigchld->tstamp, &diff);
	if (diff.tv_sec >= 2) { /* TODO acerion 2024.01.01: make the comparison more precise. Compare against 1.5 second. */
		test_log_err("Duration of exit was longer than expected: %ld.%09ld [seconds]\n", diff.tv_sec, diff.tv_nsec);
		return -1;
	}
	test_log_info("cwdaemon server exited in expected amount of time: %ld.%09ld [seconds]\n", diff.tv_sec, diff.tv_nsec);




	test_log_info("Evaluation of test events was successful %s\n", "");

	return 0;
}

