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
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <errno.h>

#include "src/cwdaemon.h"
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
	const char * description;  /** Human-readable description of the test case. */
	const char * message;      /** Message to be played by cwdaemon. */
	bool expected_fail;        /** Whether we expect cwdaemon to fail to start correctly due to invalid port number. */
	int port;                  /** Value of port passed to cwdaemon. */
} test_case_t;




static test_case_t g_test_cases[] = {
	{ .description = "failure case: port 0",        .message = "paris",  .expected_fail = true,   .port = -1 }, /* port == -1 will be interpreted by code in process.c as "pass port 0 to cwdaemon". */
	{ .description = "failure case: port 1",        .message = "paris",  .expected_fail = true,   .port = 1  },
	{ .description = "failure case: port MIN - 2",  .message = "paris",  .expected_fail = true,   .port = CWDAEMON_NETWORK_PORT_MIN - 2 },
	{ .description = "failure case: port MIN - 1",  .message = "paris",  .expected_fail = true,   .port = CWDAEMON_NETWORK_PORT_MIN - 1 },

	/* All valid ports between MIN and MAX are indirectly tested by other
	   functional tests that use random valid port. Here we just explicitly
	   test the MIN and MAX itself */
	{ .description = "success case: port MIN",      .message = "paris",  .expected_fail = false,  .port = CWDAEMON_NETWORK_PORT_MIN     },
	{ .description = "success case: port MAX",      .message = "paris",  .expected_fail = false,  .port = CWDAEMON_NETWORK_PORT_MAX     },

	{ .description = "failure case: port MAX + 1",  .message = "paris",  .expected_fail = true,   .port = CWDAEMON_NETWORK_PORT_MAX + 1 },
	{ .description = "failure case: port MAX + 2",  .message = "paris",  .expected_fail = true,   .port = CWDAEMON_NETWORK_PORT_MAX + 2 },
};




static int events_evaluate(const events_t * events, const test_case_t * test_case);
static int run_test_case(const test_case_t * test_case);




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




static int save_exit_to_events(const child_exit_info_t * child_exit_info)
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
		events_insert_sigchld_event(&g_events, child_exit_info);
	}

	return 0;
}





static int run_test_case(const test_case_t * test_case)
{
	bool failure = false;

	int wpm = 10;
	/* Remember that some receive timeouts in tests were selected when the
	   wpm was hardcoded to 10 wpm. Picking values lower than 10 may lead to
	   overrunning the timeouts. */
	cwdaemon_random_uint(10, 15, (unsigned int *) &wpm);

	const morse_receiver_config_t morse_config = { .wpm = wpm };
	cwdaemon_server_t server = { 0 };
	client_t client = { 0 };
	morse_receiver_t * morse_receiver = NULL;
	const cwdaemon_opts_t cwdaemon_opts = {
		.tone           = 640,
		.sound_system   = CW_AUDIO_SOUNDCARD,
		.nofork         = true,
		.cwdevice_name  = TEST_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
		.l4_port        = test_case->port,
	};

	const int retv = server_start(&cwdaemon_opts, &server);
	if (0 != retv) {
		save_exit_to_events(&g_child_exit_info);
		if (test_case->expected_fail) {
			return 0;
		} else {
			test_log_err("Unexpected failure to start cwdaemon with valid port %d\n", test_case->port);
			return -1;
		}
	} else {
		if (test_case->expected_fail) {
			test_log_err("Unexpected success in starting cwdaemon with invalid port %d\n", test_case->port);
			return -1;
		} else {
			; /* Go to next testing phase. */
		}
	}


	if (0 != client_connect_to_server(&client, server.ip_address, server.l4_port)) {
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server %s\n", "");
		failure = true;
		goto cleanup;
	}


	morse_receiver = morse_receiver_ctor(&morse_config);
	if (0 != morse_receiver_start(morse_receiver)) {
		fprintf(stderr, "[EE] Failed to start Morse receiver\n");
		failure = true;
		goto cleanup;
	}

	/* Send the message to be played to double-check that a cwdaemon server
	   is running, and that it's listening on a network socket. */
	client_send_request_va(&client, CWDAEMON_REQUEST_MESSAGE, "one %s", test_case->message);

	morse_receiver_wait(morse_receiver);
	morse_receiver_dtor(&morse_receiver);



	/* Terminate local test instance of cwdaemon. */
	if (0 != local_server_stop(&server, &client)) {
		/*
		  Stopping a server is not a main part of a test, but if a
		  server can't be closed then it means that the main part of the
		  code has left server in bad condition. The bad condition is an
		  indication of an error in tested functionality. Therefore set
		  failure to true.
		*/
		test_log_err("Failed to correctly stop local test instance of cwdaemon at end of test case %s\n", "");
		failure = true;
	}
	save_exit_to_events(&g_child_exit_info);

	/* Close our socket to cwdaemon server. */
	client_disconnect(&client);

	if (failure) {
		fprintf(stderr, "[EE] Test case failed, terminating %s\n", "");
		exit(EXIT_FAILURE);
	} else {
		fprintf(stderr, "[II] Test case succeeded %s\n", "");
	}


#if 0
		/* There was never a signal from child (at least not in
		   reasonable time. This will be recognized by
		   events_evaluate(). */
	}
#endif


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
	/* Expectation 1: count of events. */
	if (test_case->expected_fail) {
		/* We only expect "exit" event to be recorded. */
		if (1 != events->event_idx) {
			test_log_err("Failure case: incorrect count of events: %d\n", events->event_idx);
			return -1;
		}
	} else {
		/* We expect two events: first a Morse receive event (because
		   cwdaemon was running and listening on valid port), and then a
		   clean "exit" event. */
		if (2 != events->event_idx) {
			test_log_err("Success case: incorrect count of events: %d\n", events->event_idx);
			return -1;
		}
	}
	test_log_info("Expectation 1: found expected count of events: %d\n", events->event_idx);




	/* Expectation 2: correct event types. */
	const event_t * morse_event = NULL;
	const event_t * sigchld_event = NULL;

	if (test_case->expected_fail) {
		int i = 0;
		if (events->events[i].event_type != event_type_sigchld) {
			test_log_err("Failure case: event #%d has unexpected type: %d\n", i, events->events[i].event_type);
			return -1;
		}
		sigchld_event = &events->events[i];
	} else {
		int i = 0;
		if (events->events[i].event_type != event_type_morse_receive) {
			test_log_err("Success case: event #%d has unexpected type: %d\n", i, events->events[i].event_type);
			return -1;
		}
		morse_event = &events->events[i];

		i++;

		if (events->events[i].event_type != event_type_sigchld) {
			test_log_err("Success case: event #%d has unexpected type: %d\n", i, events->events[i].event_type);
			return -1;
		}
		sigchld_event = &events->events[i];
	}
	test_log_info("Expectation 2: found expected types of events %s\n", "");




	/* Expectation 3: correct contents of events. */
	const int wstatus = sigchld_event->u.sigchld.wstatus;
	if (test_case->expected_fail) {
		if (!WIFEXITED(wstatus)) {
			test_log_err("Failure case: cwdaemon did not exit, wstatus = 0x%04x\n", wstatus);
			return -1;
		}
		if (EXIT_FAILURE != WEXITSTATUS(wstatus)) {
			test_log_err("Success case: incorrect exit status (expected 0): 0x%04x\n", WEXITSTATUS(wstatus));
			return -1;
		}
		test_log_info("Expectation 3: failure case: exit status was correct (0x%04x)\n", wstatus);
	} else {
		if (!morse_receive_text_is_correct(morse_event->u.morse_receive.string, test_case->message)) {
			test_log_err("Success case: Invalid received string [%s] doesn't match [%s]\n",
			             morse_event->u.morse_receive.string, test_case->message);
			return -1;
		}
		test_log_info("Expectation 3a: success case: received Morse test was correct: [%s]\n", morse_event->u.morse_receive.string);

		if (!WIFEXITED(wstatus)) {
			test_log_err("Success case: cwdaemon did not exit, wstatus = 0x%04x\n", wstatus);
			return -1;
		}
		if (EXIT_SUCCESS != WEXITSTATUS(wstatus)) {
			test_log_err("Success case: incorrect exit status (expected 0/EXIT_SUCCESS): 0x%04x\n", WEXITSTATUS(wstatus));
			return -1;
		}
		test_log_info("Expectation 3b: success case: exit status was correct (0x%04x)\n", wstatus);
	}




	test_log_info("Evaluation of test events was successful %s\n", "");

	return 0;
}

