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




#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcw.h>

#include "src/cwdaemon.h"
#include "tests/library/events.h"
#include "tests/library/expectations.h"
#include "tests/library/log.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/server.h"
#include "tests/library/string_utils.h"

#include "shared.h"




/// @file
///
/// Code shared between basic tests of SOUND_SYSTEM Escape request and (in the
/// future) non-basic tests of the request.




static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, test_options_t const * test_opts, enum cw_audio_systems initial_sound_system);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_run(tests_sound_systems_available_t const * avail, test_case_t const * test_case, client_t * client, morse_receiver_t * morse_receiver, events_t * events, enum cw_audio_systems initial_sound_system);
static int evaluate_events(events_t const * recorded_events, test_case_t const * test_case);




// @reviewed_on{2024.05.22}
int run_test_cases(test_case_t const * test_cases, size_t n_test_cases, test_options_t const * test_opts, char const * test_name)
{
	tests_sound_systems_available_t avail = { 0 };
	tests_sound_systems_availability(&avail);

	// We know that there is only one test case. Specification of tested
	// sound systems isn't part of set of test cases.
	if (1 != n_test_cases) {
		test_log_err("Test: unexpected count of test cases: is %zu, expected 1\n", n_test_cases);
		return -1;
	}
	test_case_t const * test_case = &test_cases[0];


	bool failure = false;
	events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
	server_t server = { .events = &events };
	client_t client = { .events = &events };
	morse_receiver_t morse_receiver = { .events = &events };

	// Sound system with which cwdaemon will be started. cwdaemon won't be
	// playing Morse code with this particular sound system because another
	// sound system will be picked at random at the beginning of each test
	// cycle.
	enum cw_audio_systems initial_sound_system = test_opts->sound_system;
	if (CW_AUDIO_NONE == initial_sound_system) {
		// Initial sound system was not specified through command line
		// option, so pick the initial sound system here.
		if (0 != tests_pick_random_sound_system(&avail, &initial_sound_system)) {
			test_log_err("Test: failed to pick initial sound system %s\n", "");
			return -1;
		}
	}
	test_log_info("Test: initial sound system is [%s]\n", tests_get_sound_system_label_long(initial_sound_system));


	if (0 != test_setup(&server, &client, &morse_receiver, test_opts, initial_sound_system)) {
		test_log_err("Test: failed at test setup for [%s] test\n", test_name);
		failure = true;
		goto cleanup;
	}

	if (0 != test_run(&avail, test_case, &client, &morse_receiver, &events, initial_sound_system)) {
		test_log_err("Test: failed at running test cases for [%s] test\n", test_name);
		failure = true;
		goto cleanup;
	}

 cleanup:
	if (0 != test_teardown(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at test tear down for [%s] test\n", test_name);
		failure = true;
	}

	if (failure) {
		test_log_err("Test: FAIL ([%s] test)\n", test_name);
		test_log_newline(); /* Visual separator. */
		return -1;
	}
	test_log_info("Test: PASS ([%s] test)\n", test_name);
	test_log_newline(); /* Visual separator. */
	return 0;
}




/// @brief Evaluate events that were recorded during execution of single test case
///
/// Look at contents of @p events and check if order and types of events are as expected.
///
/// The events may include
///  - receiving Morse code
///  - receiving reply from cwdaemon server,
///  - changes of state of PTT pin,
///  - exiting of local instance of cwdaemon server process,
///
/// @reviewed_on{2024.05.20}
///
/// @return 0 if events are in proper order and of proper type
/// @return -1 otherwise
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


	// Expectation: recorded Morse event and reply event are close enough to
	// each other. Check distance of the two events on time axis.
	expectation_idx = 2;
	if (0 != expect_morse_and_reply_events_distance(expectation_idx, recorded)) {
		return -1;
	}


	test_log_info("Test: evaluation of test events was successful for test case [%s]\n", test_case->description);

	return 0;
}




/// @brief Prepare resources used to execute set of test cases
///
/// @reviewed_on{2024.05.22}
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, test_options_t const * test_opts, enum cw_audio_systems initial_sound_system)
{
	const int wpm = tests_get_test_wpm();

	// Prepare local test instance of cwdaemon server.
	const server_options_t server_opts = {
		.tone           = tests_get_test_tone(),
		.sound_system   = initial_sound_system,
		.cwdevice_name  = TESTS_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
		.supervisor_id  = test_opts->supervisor_id,
		.log_threshold  = LOG_INFO,
	};
	if (0 != server_start(&server_opts, server)) {
		test_log_err("Test: failed to start cwdaemon server %s\n", "");
		return -1;
	}


	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { // TODO (acerion) 2024.05.14: remove casting.
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		return -1;
	}
	client_socket_receive_enable(client);
	if (0 != client_socket_receive_start(client)) {
		test_log_err("Test: failed to start socket receiver %s\n", "");
		return -1;
	}


	const morse_receiver_config_t morse_config = { .wpm = wpm };
	if (0 != morse_receiver_configure(&morse_config, morse_receiver)) {
		test_log_err("Test: failed to configure Morse receiver %s\n", "");
		return -1;
	}


	return 0;
}




/// @brief Clean up resources used to execute set of test cases
///
/// @reviewed_on{2024.05.20}
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
		test_log_err("Test: failed to correctly stop local test instance of cwdaemon %s\n", "");
		failure = true;
	}

	morse_receiver_deconfigure(morse_receiver);

	client_socket_receive_stop(client);
	client_disconnect(client);
	client_dtor(client);

	return failure ? -1 : 0;
}




/// @brief Build a SOUND SYSTEM Escape request
///
/// Fill given @p request with bytes that make a proper SOUND_SYSTEM Escape
/// request. Put there a value that will make cwdaemon switch to sound system
/// specified by @p sound_system.
///
/// The value (an array of bytes) is or is not terminated by NUL - this is
/// decided at random. cwdaemon server should be able to safely handle both
/// cases.
///
/// 'n_bytes' member of @p request is set according to count of bytes (with
/// or without NUL) put into the request.
///
/// @reviewed_on{2024.05.24}
///
/// @param[in] sound_system Sound system which should be requested by this request
/// @param[out] request SOUND_SYSTEM Escape request built by this function
///
/// @return 0 when request was built without problems
/// @return -1 otherwise
static int build_request(enum cw_audio_systems sound_system, test_request_t * request)
{
	// Generate value.
	char const * sound_system_label = tests_get_sound_system_label_short(sound_system);

#if 0
	// TODO (acerion) 2024.05.24: enable this block of code, but only after
	// you address a FIXME inside of it.
	char random_invalid_label[16] = { 0 };
	if (sound_system == CW_AUDIO_NONE) {
		// NONE -> send request with invalid sound system
		bool random_invalid = false;
		if (0 != cwdaemon_random_bool(&random_invalid)) {
			test_log_err("Test: failed to generate random boolean for 'random invalid' flag %s\n", "");
			return -1;
		}
		if (random_invalid) {
			// FIXME (acerion) 2024.05.22: if random chars array generated
			// here begins with n/c/o/a/p/s character, then such random array
			// will be interpreted by cwdaemon as valid sound system name -
			// because of that first character. So an array of random
			// characters isn't always invalid.
			size_t const n = sizeof (random_invalid_label);
			if (0 != cwdaemon_random_printable_characters(random_invalid_label, n)) {
				test_log_err("Test: failed to generate array of printable characters for 'random invalid label' %s\n", "");
				return -1;
			}
			random_invalid_label[n - 1] = '\0';
			sound_system_label = random_invalid_label;
		} else {
			// Use a constant, non-random, hardcoded short value returned by
			// tests_get_sound_system_label_short().
		}
	}
#endif

	// Generate count of bytes in value.
	size_t val_n_bytes = strlen(sound_system_label);
	bool with_nul = false;
	if (0 != cwdaemon_random_bool(&with_nul)) {
		test_log_err("Test: failed to decide if we want to append terminating NUL to [%s] sound system label\n", sound_system_label);
		return -1;
	}
	if (with_nul) {
		// Also include terminating NUL in a label of sound system sent in
		// the request.
		val_n_bytes++;
	}

	// Build request's bytes and n_bytes.
	request->n_bytes = 0;
	request->bytes[request->n_bytes++] = ASCII_ESC;
	request->bytes[request->n_bytes++] = CWDAEMON_ESC_REQUEST_SOUND_SYSTEM;
	for (size_t i_val = 0; i_val < val_n_bytes; i_val++) {
		request->bytes[request->n_bytes++] = sound_system_label[i_val];
	}

#if 1 // Only for debug.
	char printable[PRINTABLE_BUFFER_SIZE(sizeof ("033fLongLabelOfSoundSystem"))] = { 0 };
	get_printable_string(request->bytes, request->n_bytes, printable, sizeof (printable));
	test_log_debug("Test: Generated %zu bytes of request: [%s]\n", request->n_bytes, printable);
#endif

	return 0;
}




/// @brief Run all test cases. Evaluate results (the events) of each test case.
///
/// @reviewed_on{2024.05.22}
static int test_run(tests_sound_systems_available_t const * avail, test_case_t const * test_case, client_t * client, morse_receiver_t * morse_receiver, events_t * events, enum cw_audio_systems initial_sound_system)
{
	bool failure = false;

	// We want to allow generating Escape requests with invalid sound system
	// "none" to see how cwdaemon will handle the requests.
	tests_sound_systems_available_t avail_with_none = *avail;
	avail_with_none.none_available = true;

	// Sound system from which we move away in this test case. Sometimes a
	// new requested sound system will be invalid (CW_AUDIO_NONE/"x") -
	// cwdaemon should ignore such requests and continue with its current
	// sound system.
	enum cw_audio_systems old_sound_system = initial_sound_system;

	const size_t n_iterations = 20;

	for (size_t iter = 0; iter < n_iterations; iter++) {
		test_log_newline(); // Visual separator.
		test_log_info("Test: starting iteration %zu / %zu\n\n", iter + 1, n_iterations);

		enum cw_audio_systems new_sound_system = CW_AUDIO_NONE;
		if (0 != tests_pick_random_sound_system(&avail_with_none, &new_sound_system)) {
			return -1;
		}

		test_request_t request = { 0 };
		if (0 != build_request(new_sound_system, &request)) {
			return -1;
		}

		char const * const old_sound_system_label = tests_get_sound_system_label_long(old_sound_system);
		if (NULL == old_sound_system_label) {
			return -1;
		}

		char const * const new_sound_system_label = tests_get_sound_system_label_long(new_sound_system);
		if (NULL == new_sound_system_label) {
			return -1;
		}


		// This is needed to give tester possibility to recognize if/when to
		// expect sound and where from. Sometimes there will be a Null sound
		// system, and sometimes there will be a sound generated by PC
		// buzzer. Tester must know when to expect a sound, and when to
		// expect no sound.
		test_log_info("Test: this test case will try switching sound system: [%s] ----> [%s]\n", old_sound_system_label, new_sound_system_label);
		test_log_info("Test: press any key to run the test case %s\n", "");

		// gcc 13.2.0 on FreeBSD 14.1 complains about getchar():
		// "warning: assuming signed overflow does not occur when changing X +- C1 cmp C2 to X cmp C2 -+ C1 [-Wstrict-overflow]"
		// The warning is reported for function's closing brace.
		getchar();

		// This is the actual test.
		{
			if (0 != morse_receiver_start(morse_receiver)) {
				test_log_err("Test: failed to start Morse receiver in iteration %zu\n", iter);
				return -1;
			}

			// Tell cwdaemon server to switch to a new sound system.
			if (0 != client_send_request(client, &request)) {
				test_log_err("Test: failed to send SOUND_SYSTEM Escape request with sound system [%s]\n", new_sound_system_label);
				return -1;
			}

			// TODO (acerion) 2024.05.14: introduce random sleep between
			// SOUND_SYSTEM and REPLY Escape requests?

			// Now we ask cwdaemon to remember a reply that should be sent
			// back to us after a message is played.
			//
			// Then we send the message itself.
			//
			// Then we wait for completion of job by:
			//  - Morse receiver thread that decodes a Morse code on cwdevice - there is an explicit wait below
			//  - socket receiver that receives the remembered reply - there is an implicit wait behind the scenes.

			// Ask cwdaemon to send us this reply back after playing a
			// message.
			client_send_request(client, &test_case->reply_esc_request);

			// Send PLAIN message to be played and keyed on cwdevice and
			// played through sound system.
			client_send_request(client, &test_case->plain_request);

			// Receive events on cwdevice (Morse code on keying pin AND/OR
			// ptt events on ptt pin).
			morse_receiver_wait_for_stop(morse_receiver);

			// A reply has been received implicitly by client for which we
			// called client_socket_receive_enable()/start(). FIXME (acerion)
			// 2024.05.14: shouldn't we explicitly wait here also for receipt
			// of reply? Maybe some sleep here?
		}


		// Validation of events that were recorded during test.
		events_sort(events);
		if (0 != evaluate_events(events, test_case)) {
			test_log_err("Test: evaluation of events has failed in iteration %zu / %zu for test case [%s]\n", iter + 1, n_iterations, test_case->description);
			failure = true;
			break;
		}
		// Clear stuff before running next test case.
		events_clear(events);

		test_log_info("Test: iteration %zu / %zu: test case [%s] has succeeded\n\n", iter + 1, n_iterations, test_case->description);

		if (new_sound_system != CW_AUDIO_NONE) {
			// cwdaemon switched to new sound system only if the new
			// system is valid (i.e. is not CW_AUDIO_NONE).
			old_sound_system = new_sound_system;
		}
	}

	return failure ? -1 : 0;
}




