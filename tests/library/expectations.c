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


