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
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




#include <stdio.h>

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




int expect_reply_match(int expectation_idx, const test_reply_data_t * received, const test_reply_data_t * expected)
{
	// This part implements checking of expectation.
	const bool correct = socket_receive_bytes_is_correct(expected, received);

	// This part implements ONLY logging.
	{
		char printable_received[PRINTABLE_BUFFER_SIZE(sizeof (received->bytes))] = { 0 };
		get_printable_string(received->bytes, received->n_bytes, printable_received, sizeof (printable_received));

		char printable_expected[PRINTABLE_BUFFER_SIZE(sizeof (expected->bytes))] = { 0 };
		get_printable_string(expected->bytes, expected->n_bytes, printable_expected, sizeof (printable_expected));

		if (correct) {
			test_log_info("Expectation %d: received reply %zu/[%s] matches expected reply %zu/[%s]\n",
			              expectation_idx,
			              received->n_bytes, printable_received,
			              expected->n_bytes, printable_expected);
		} else {
			test_log_err("Expectation %d: received reply %zu/[%s] doesn't match expected reply %zu/[%s]\n",
			             expectation_idx,
			             received->n_bytes, printable_received,
			             expected->n_bytes, printable_expected);
		}
	}

	return correct ? 0 : -1;
}




int expect_morse_match(int expectation_idx, event_morse_receive_t const * received, const char * expected)
{
	// This part implements checking of expectation.
	const bool correct = morse_receive_text_is_correct(received->string, expected);

	// This part implements ONLY logging.
	{
		if (correct) {
			test_log_info("Expectation %d: received Morse message [%s] matches expected Morse message [%s] (ignoring the first character)\n",
			              expectation_idx, received->string, expected);
		} else {
			test_log_err("Expectation %d: received Morse message [%s] doesn't match expected Morse message [%s]\n",
			             expectation_idx, received->string, expected);
		}
	}

	return correct ? 0 : -1;
}




int expect_morse_and_reply_events_distance(int expectation_idx, bool morse_is_earlier, const event_t * morse_event, const event_t * reply_event)
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




int expect_morse_and_reply_events_distance2(int expectation_idx, event_t const * recorded_events)
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
		if (0 != expect_morse_and_reply_events_distance(expectation_idx, morse_is_earlier, morse_event, reply_event)) {
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




int expect_morse_and_reply_events_order(int expectation_idx, bool morse_is_earlier)
{
	// This part implements checking of expectation.
	const bool correct = morse_is_earlier;

	// This part implements ONLY logging.
	{
		if (correct) {
			// This would be the correct order of events, but currently
			// (cwdaemon 0.11.0, 0.12.0) this is not the case: the order of
			// events is reverse. Right now I'm not willing to fix it yet.
			//
			// TODO (acerion) 2023.12.30: fix the order of the two events in
			// cwdaemon. At the very least decrease the time difference
			// between the events from current ~300ms to few ms.
			test_log_warn("Expectation %d: unexpected order of events: Morse -> socket\n", expectation_idx);
		} else {
			// This is the current incorrect behaviour that is accepted for
			// now.
			test_log_warn("Expectation %d: incorrect (but currently expected) order of events: socket -> Morse\n", expectation_idx);
		}
	}

	// Always return zero because I don't know what is the truly correct
	// result. TODO (acerion) 2024.01.26: start returning -1 at some point in
	// the future, after your are certain of correct order of events.
	return 0;
}




int expect_count_of_events(int expectation_idx, int n_recorded, int n_expected)
{
	// This part implements checking of expectation.
	const bool correct = n_recorded == n_expected;

	// This part implements ONLY logging.
	{
		if (correct) {
			test_log_info("Expectation %d: found expected count of events: %d\n", expectation_idx, n_recorded);
		} else {
			test_log_err("Expectation %d: unexpected count of events: recorded %d events, expected %d events\n",
			             expectation_idx, n_recorded, n_expected);
		}
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
			                             expected[i].u.morse.string)) {
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

		case etype_req_exit:
		case etype_sigchld:
		default:
			test_log_err("Expectation %d: unhandled event type %u at position %d\n",
			             expectation_idx, recorded[i].etype, i);
			return -1;
		}
	}
	test_log_info("Expectation %d: found expected count of events, with proper types, in proper order, with proper contents\n", expectation_idx);

	return 0;
}

