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
   Test for proper re-registration of libcw keying callback when handling RESET
   request. https://github.com/acerion/cwdaemon/issues/6

   See also "#define CWDAEMON_GITHUB_ISSUE_6_FIXED" in src/cwdaemon.c.
*/




#define _DEFAULT_SOURCE




#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tests/library/client.h"
#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/events.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/random.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"
#include "tests/library/thread.h"




events_t g_events = { .mutex = PTHREAD_MUTEX_INITIALIZER };




static int evaluate_events(events_t * events, const char * message1, const char * message2);




int main(void)
{
#if 0
	if (!test_env_is_usable(test_env_libcw_without_signals)) {
		test_log_err("Preconditions for test env are not met, exiting %s\n", "");
		exit(EXIT_FAILURE);
	}
#endif

	const uint32_t seed = cwdaemon_srandom(0);
	test_log_debug("Random seed: 0x%08x (%u)\n", seed, seed);

	int wpm = 10;
	/* Remember that some receive timeouts in tests were selected when the
	   wpm was hardcoded to 10 wpm. Picking values lower than 10 may lead to
	   overrunning the timeouts. */
	cwdaemon_random_uint(10, 15, (unsigned int *) &wpm);

	const morse_receiver_config_t morse_config = { .wpm = wpm };
	morse_receiver_t * morse_receiver = morse_receiver_ctor(&morse_config);
	const char * message1 = "paris";
	const char * message2 = "texas";

	bool failure = false;
	const cwdaemon_opts_t cwdaemon_opts = {
		.tone               = 630,
		.sound_system       = CW_AUDIO_SOUNDCARD,
		.nofork             = true,
		.cwdevice_name      = TEST_TTY_CWDEVICE_NAME,
		.wpm                = wpm,
	};

	cwdaemon_server_t server = { 0 };
	client_t client = { 0 };
	if (0 != server_start(&cwdaemon_opts, &server)) {
		test_log_err("Failed to start cwdaemon server, exiting %s\n", "");
		failure = true;
		goto cleanup;
	}


	if (0 != client_connect_to_server(&client, server.ip_address, server.l4_port)) {
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server %s\n", "");
		failure = true;
		goto cleanup;
	}


	/* This sends a text request to cwdaemon server that works in initial state,
	   i.e. reset command was not sent yet, so cwdaemon should not be
	   broken yet. */
	{
		if (0 != morse_receiver_start(morse_receiver)) {
			test_log_err("Failed to start Morse receiver (%d)\n", 1);
			failure = true;
			goto cleanup;
		}

		client_send_request_va(&client, CWDAEMON_REQUEST_MESSAGE, "one %s", message1);

		morse_receiver_wait(morse_receiver);
	}



	/* This would break the cwdaemon before a fix to
	   https://github.com/acerion/cwdaemon/issues/6 was applied. */
	client_send_request(&client, CWDAEMON_REQUEST_RESET, "");


	/* This sends a text request to cwdaemon that works in "after reset"
	   state. A fixed cwdaemon should reset itself correctly. */
	{
		if (0 != morse_receiver_start(morse_receiver)) {
			test_log_err("Failed to start Morse receiver (%d)\n", 2);
			failure = true;
			goto cleanup;
		}

		client_send_request_va(&client, CWDAEMON_REQUEST_MESSAGE, "one %s", message2);

		morse_receiver_wait(morse_receiver);
	}



	if (0 != evaluate_events(&g_events, message1, message2)) {
		test_log_err("Evaluation of events has failed %s\n", "");
		failure = true;
	} else {
		test_log_info("Evaluation of events was successful %s\n", "");
	}




 cleanup:
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
		test_log_err("Failed to correctly stop local test instance of cwdaemon server %s\n", "");
		failure = true;
	}

	/*
	  Close our socket to cwdaemon server. cwdaemon may be stopped, but let's
	  still try to close socket on our end.
	*/
	client_disconnect(&client);

	if (failure) {
		test_log_err("Test case %d/%d failed, terminating\n", 1, 1);
		exit(EXIT_FAILURE);
	} else {
		test_log_info("Test case %d/%d succeeded\n\n", 1, 1);
		exit(EXIT_SUCCESS);
	}
}




static int evaluate_events(events_t * events, const char * message1, const char * message2)
{
	/* Expectation 1: there are two events: Morse code was keyed (and received) on cwdevice twice. */
	if (2  != events->event_idx) {
		test_log_err("Incorrect count of events: %d\n", events->event_idx);
		return -1;
	}
	test_log_info("Correct count of test events: %d\n", events->event_idx);




	/* Expectation 2: both events are of type "Morse receive". */
	event_t * morse1 = &events->events[0];
	event_t * morse2 = &events->events[1];
	if (morse1->event_type != event_type_morse_receive
	    || morse2->event_type != event_type_morse_receive) {

		test_log_err("Incorrect type of event(s): %d, %d\n", morse1->event_type, morse2->event_type);
		return -1;
	}
	test_log_info("Correct types of test events: %d, %d\n", morse1->event_type, morse2->event_type);




	/* Expectation 3: both Morse receive events contain correct received text. */
	const char * received_string1 = morse1->u.morse_receive.string;
	const char * received_string2 = morse2->u.morse_receive.string;
	if (!morse_receive_text_is_correct(received_string1, message1)
	    || !morse_receive_text_is_correct(received_string2, message2)) {

		test_log_err("Incorrect text in Morse receive event(s): [%s], [%s]\n",
		             received_string1, received_string2);
		return -1;
	}
	test_log_info("Correct text in Morse receive events: [%s], [%s]\n",
	              received_string1, received_string2);




	test_log_info("Evaluation of test events was successful %s\n", "");

	return 0;
}

