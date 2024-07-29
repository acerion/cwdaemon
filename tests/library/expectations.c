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
#include <sys/wait.h>

#include "events.h"
#include "expectations.h"
#include "log.h"
#include "morse_receiver_utils.h"
#include "string_utils.h"
#include "time_utils.h"




/// @file
///
/// What is expected of events occurring during tests? Do recorded events
/// meet the expectations planned in test scenarios? Do the recorded events
/// match expected events?




static int expect_morse_match(int expectation_idx, event_morse_receive_t const * received, event_morse_receive_t const * expected);
static int expect_morse_and_reply_events_distance_inner(int expectation_idx, bool morse_is_earlier, const event_t * morse_event, const event_t * reply_event);




/// @brief Data received in "reply" from cwdaemon must match data sent in
/// "reply" Escape request or "caret" request
///
/// @reviewed_on{2024.07.28}
///
/// @param[in] expectation_idx Index/number of expectation - info to be included in logs
/// @param[in] received Reply received from tested cwdaemon server
/// @param[in] expected Expected reply - what test expects to receive from tested cwdaemon server
///
/// @return 0 if expectation is met
/// @return -1 otherwise
static int expect_reply_match(int expectation_idx, const test_reply_data_t * received, const test_reply_data_t * expected)
{
	// This part implements checking if expectation is met.
	const bool correct = socket_receive_bytes_is_correct(expected, received);

	// This part implements ONLY logging.
	{
		char printable_received[PRINTABLE_BUFFER_SIZE(sizeof (received->bytes))] = { 0 };
		get_printable_string(received->bytes, received->n_bytes, printable_received, sizeof (printable_received));

		char printable_expected[PRINTABLE_BUFFER_SIZE(sizeof (expected->bytes))] = { 0 };
		get_printable_string(expected->bytes, expected->n_bytes, printable_expected, sizeof (printable_expected));

		// Print the two messages (expected and received) as strings aligned
		// horizontally, to make their visual comparison easier.
		test_log_info("Expectation %d: expected reply %zu/[%s]\n", expectation_idx, expected->n_bytes, printable_expected);
		test_log_info("Expectation %d: received reply %zu/[%s]\n", expectation_idx, received->n_bytes, printable_received);

		if (correct) {
			test_log_info("Expectation %d: received reply matches expected reply\n", expectation_idx);
		} else {
			test_log_err("Expectation %d: received reply doesn't match expected reply\n", expectation_idx);
		}
	}

	return correct ? 0 : -1;
}




/**
   @brief Text keyed on cwdevice and received by Morse receiver must match text sent to cwdaemon for keying

   Receiving of message by Morse receiver should not be verified if the
   expected message is too short (the problem with "warm-up" of receiver).

   @reviewed_on{2024.07.28}

   @param[in] expectation_idx Index/number of expectation - info to be included in logs
   @param[in] received Morse message keyed by cwdaemon and received on cwdevice by Morse receiver
   @param[in] expected Expected Morse message - what test expects to be keyed by tested cwdaemon server

   @return 0 if expectation is met
   @return -1 otherwise
*/
static int expect_morse_match(int expectation_idx, event_morse_receive_t const * received, event_morse_receive_t const * expected)
{
	// This part implements checking if expectation is met.
	const bool correct = morse_receive_text_is_correct(received->string, expected->string);

	// This part implements ONLY logging.
	{
		// Print the two messages (expected and received) as strings aligned
		// horizontally, to make their visual comparison easier.
		test_log_info("Expectation %d: expected Morse message [%s]\n", expectation_idx, expected->string);
		test_log_info("Expectation %d: received Morse message [%s]\n", expectation_idx, received->string);

		if (correct) {
			test_log_info("Expectation %d: received Morse message matches expected Morse message (ignoring the first character)\n", expectation_idx);
		} else {
			test_log_err("Expectation %d: received Morse message doesn't match expected Morse message\n", expectation_idx);
		}
	}

	return correct ? 0 : -1;
}




/**
   @brief The end of receiving of Morse message and the time of receiving a
   reply should be separated by short time span

   Evaluate time span between 'reply' event and the end of receiving a Morse
   message.

   Currently (0.12.0) the time span is ~300ms. TODO acerion 2023.12.31:
   shorten the time span in cwdaemon.

   @reviewed_on{2024.02.19}

   @param[in] expectation_idx Index/number of expectation - info to be included in logs
   @param[in] morse_is_earlier Whether Morse event is earlier than reply event
   @param[in] morse_event Event describing receiving of Morse message on cwdevice
   @param[in] reply_event Event describing receiving a reply from tested cwdaemon server

   @return 0 if expectation is met
   @return -1 otherwise
*/
static int expect_morse_and_reply_events_distance_inner(int expectation_idx, bool morse_is_earlier, const event_t * morse_event, const event_t * reply_event)
{
	struct timespec diff = { 0 };
	if (morse_is_earlier) {
		timespec_diff(&morse_event->tstamp, &reply_event->tstamp, &diff);
	} else {
		timespec_diff(&reply_event->tstamp, &morse_event->tstamp, &diff);
	}
	// Notice that the time diff may depend on Morse code speed (wpm).
	const int threshold = 500L * 1000 * 1000; /* [nanoseconds] */
	const bool correct = diff.tv_sec == 0 && diff.tv_nsec < threshold;

	// This part implements ONLY logging.
	{
		if (correct) {
			test_log_info("Expectation %d: time difference between end of 'Morse receive' and receiving reply is ok: %ld.%09ld seconds, with threshold of 0.%09d\n",
			              expectation_idx, diff.tv_sec, diff.tv_nsec, threshold);
		} else {
			test_log_err("Expectation %d: time difference between end of 'Morse receive' and receiving reply is too large: %ld.%09ld seconds, with threshold of 0.%09d\n",
			             expectation_idx, diff.tv_sec, diff.tv_nsec, threshold);
		}
	}

	return correct ? 0 : -1;
}




int expect_morse_and_reply_events_distance(int expectation_idx, event_t const * recorded_events)
{
	// First find the two events, and then check their distance on time axis.
	///
	// TODO (acerion) 2024.05.01: the code that searches for the two events
	// can't handle a situation where there is more than one event of given
	// type. For now we don't have such tests in which we expect to record
	// more than one Morse event or more than one reply event, so it's not a
	// huge problem right now.
	event_t const * morse_event = NULL;
	event_t const * reply_event = NULL;
	int morse_idx = -1;
	int reply_idx = -1;

	for (int i = 0; i < EVENTS_MAX; i++) {
		switch (recorded_events[i].etype) {
		case etype_morse:
			morse_event = &recorded_events[i];
			morse_idx = i;
			break;
		case etype_reply:
			reply_event = &recorded_events[i];
			reply_idx = i;
			break;
		case etype_none:
		case etype_req_exit:
		case etype_sigchld:
		default:
			break;
		}
	}

	if (morse_event && reply_event) {
		const bool morse_is_earlier = morse_idx < reply_idx;
		if (0 != expect_morse_and_reply_events_distance_inner(expectation_idx, morse_is_earlier, morse_event, reply_event)) {
			return -1;
		}
	} else {
		// It's ok that one of events is not present. If we have compared
		// "expected" and "received" events using
		// expect_count_type_order_contents() before calling this function,
		// then we know that the events are not present in "recorded" on
		// purpose.
		test_log_info("Expectation %d: skipping checking of the expectation because either Morse event or reply event is not present\n", expectation_idx);
	}

	return 0;
}




int expect_exit_and_sigchld_events_distance(int expectation_idx, event_t const * recorded_events)
{
	// First find the two events, and then check their distance on time axis.
	///
	// TODO (acerion) 2024.05.03: the code that searches for the two events
	// can't handle a situation where there is more than one event of given
	// type. For now we don't have such tests in which we expect to record
	// more than one Morse event or more than one reply event, so it's not a
	// huge problem right now.

	// This part prepares data for the main part.
	const event_t * exit_request = NULL;
	const event_t * sigchld_event = NULL;
	for (int i = 0; i < EVENTS_MAX; i++) {

		/* Get references to specific events in array of events. */
		switch (recorded_events[i].etype) {
		case etype_morse:
			break;
		case etype_sigchld:
			sigchld_event = &recorded_events[i];
			break;
		case etype_req_exit:
			exit_request = &recorded_events[i];
			break;
		case etype_none:
		case etype_reply:
		default:
			break;
		}
	}
	if (NULL == sigchld_event) {
		/*  This test is to satisfy clang-tidy's check for NULL dereference. */
		test_log_err("Expectation %d: sigchld event was not found\n", expectation_idx);
		return -1;
	}


	// This part implements checking of expectation.
	struct timespec diff = { 0 };
	// TODO (acerion) 2024.04.20: do we have a guarantee that the two events
	// happened in expected order (i.e. exit request first, and sigchld
	// second)?
	timespec_diff(&exit_request->tstamp, &sigchld_event->tstamp, &diff);
	const int threshold = 2;
	const bool correct = diff.tv_sec < threshold; /* TODO acerion 2024.01.01: make the comparison more precise. Compare against 1.5 second. */


	// This part implements ONLY logging.
	if (correct) {
		test_log_info("Expectation %d: cwdaemon server exited in expected amount of time: %ld.%09ld [seconds], threshold is %d.0 [seconds]\n", expectation_idx, diff.tv_sec, diff.tv_nsec, threshold);
	} else {
		test_log_err("Expectation %d: duration of exit was longer than expected: %ld.%09ld [seconds], threshold is %d.0 [seconds]\n", expectation_idx, diff.tv_sec, diff.tv_nsec, threshold);
	}


	return correct ? 0 : -1;
}




int expect_count_type_order_contents(int expectation_idx, event_t const * expected, event_t const * recorded)
{
	// Loop condition uses EVENTS_MAX because we want to detect and compare
	// also etype_none events. See comment for line comparing event types
	// ("etype" members) just below.
	for (int i = 0; i < EVENTS_MAX; i++) {

		// A non-obvious purpose of this comparison is to detect different
		// count of events in both arrays.
		//
		// If one of events is etype_none and the other is not, then we
		// detected that "expected[]" and "recorded[]" contain different
		// count of events. That would mean that an error of cwdaemon was
		// caught.
		if (expected[i].etype != recorded[i].etype) {
			test_log_err("Expectation %d: unexpected event at position %d: expected %u, recorded %u\n",
			             expectation_idx, i,
			             expected[i].etype,
			             recorded[i].etype);
			return -1;
		}

		switch (recorded[i].etype) {
		case etype_morse:
			if (0 != expect_morse_match(expectation_idx,
			                            &recorded[i].u.morse,
			                            &expected[i].u.morse)) {
				test_log_err("Expectation %d: mismatch of 'Morse' event at position %d\n", expectation_idx, i);
				return -1;
			}
			break;

		case etype_reply:
			if (0 != expect_reply_match(expectation_idx,
			                            &recorded[i].u.reply,
			                            &expected[i].u.reply)) {
				test_log_err("Expectation %d: mismatch of 'reply' event at position %d\n", expectation_idx, i);
				return -1;
			}
			break;

		case etype_none:
			// A test at the top of the loop ensured that both "expected" and
			// "recorded" event has the same type. So if we get into this
			// "case", this means that both arrays have an empty slot here.
			// We have now gone beyond last non-empty expected+recorded event
			// and are safely and correctly comparing empty remainders of
			// both arrays.
			break;

		case etype_sigchld:
			if (WIFEXITED(recorded[i].u.sigchld.wstatus) != expected[i].u.sigchld.exp_exited) {
				test_log_err("Expectation %d: failure case: mismatch about exit(): expected %d, recorded %d\n",
				             expectation_idx,
				             WIFEXITED(recorded[i].u.sigchld.wstatus), expected[i].u.sigchld.exp_exited);
				return -1;
			}
			if (WIFEXITED(recorded[i].u.sigchld.wstatus)) {
				if (WEXITSTATUS(recorded[i].u.sigchld.wstatus) != expected[i].u.sigchld.exp_exit_arg) {
					test_log_err("Expectation %d: failure case: process exited, but exit status doesn't match: expected %d, recorded %d\n",
					             expectation_idx,
					             WEXITSTATUS(recorded[i].u.sigchld.wstatus), expected[i].u.sigchld.exp_exit_arg);
					return -1;
				}
			}
			test_log_info("Expectation %d: processes' exit status is as expected (0x%04x)\n", expectation_idx, (unsigned int) recorded[i].u.sigchld.wstatus);
			break;

		case etype_req_exit:
			// The recorded event comes from test program, not from tested
			// cwdaemon server.
			//
			// Here we just recognize here that we sent an EXIT Escape
			// request to tested server. There is nothing more to do here
			// because the event doesn't contain any details (apart from
			// timestamp may be evaluated in other function).
			test_log_info("Expectation %d: detected sending of EXIT Escape request to tested cwdaemon server\n", expectation_idx);
			break;

		default:
			test_log_err("Expectation %d: unhandled event type %u at position %d\n",
			             expectation_idx, recorded[i].etype, i);
			return -1;
		}
	}
	test_log_info("Expectation %d: found expected count of events, with proper types, in proper order, with proper contents\n", expectation_idx);

	return 0;
}

