#include <stdio.h>

#include "events.h"
#include "expectations.h"
#include "log.h"
#include "morse_receiver_utils.h"
#include "string_utils.h"
#include "time_utils.h"




int expect_socket_reply_match(int expectation_idx, const socket_receive_data_t * received, const socket_receive_data_t * expected)
{
	char printable_received[PRINTABLE_BUFFER_SIZE(sizeof (received->bytes))] = { 0 };
	get_printable_string(received->bytes, printable_received, sizeof (printable_received));

	char printable_expected[PRINTABLE_BUFFER_SIZE(sizeof (expected->bytes))] = { 0 };
	get_printable_string(expected->bytes, printable_expected, sizeof (printable_expected));

	if (!socket_receive_bytes_is_correct(expected, received)) {
		test_log_err("Expectation %d: received socket reply [%s] doesn't match expected socket reply [%s]\n",
		             expectation_idx,
		             printable_received, printable_expected);
		return -1;
	}
	test_log_info("Expectation %d: received socket reply %zu/[%s] matches expected reply %zu/[%s]\n",
	              expectation_idx,
	              received->n_bytes, printable_received,
	              expected->n_bytes, printable_expected);

	return 0;
}




int expect_morse_receive_match(int expectation_idx, const char * received, const char * expected)
{
	if (!morse_receive_text_is_correct(received, expected)) {
		test_log_err("Expectation %d: received Morse message [%s] doesn't match expected Morse message [%s]\n",
		             expectation_idx, received, expected);
		return -1;
	}
	test_log_info("Expectation %d: received Morse message [%s] matches expected Morse message [%s] (ignoring the first character)\n",
	              expectation_idx, received, expected);

	return 0;
}




int expect_morse_and_socket_events_distance(int expectation_idx, int morse_idx, const event_t * morse_event, int socket_idx, const event_t * socket_event)
{
	struct timespec diff = { 0 };
	if (morse_idx < socket_idx) {
		timespec_diff(&morse_event->tstamp, &socket_event->tstamp, &diff);
	} else {
		timespec_diff(&socket_event->tstamp, &morse_event->tstamp, &diff);
	}

	/*
	  Notice that the time diff may depend on Morse code speed (wpm).
	*/
	const int threshold = 500L * 1000 * 1000; /* [nanoseconds] */
	if (diff.tv_sec > 0 || diff.tv_nsec > threshold) {
		test_log_err("Expectation %d: time difference between end of 'Morse receive' and receiving socket reply is too large: %ld.%09ld seconds\n",
		             expectation_idx, diff.tv_sec, diff.tv_nsec);
		return -1;
	}
	test_log_info("Expectation %d: time difference between end of 'Morse receive' and receiving socket reply is ok: %ld.%09ld seconds\n",
	              expectation_idx, diff.tv_sec, diff.tv_nsec);

	return 0;
}




int expect_morse_and_socket_event_order(int expectation_idx, int morse_idx, int socket_idx)
{
	if (morse_idx < socket_idx) {
		/* This would be the correct order of events, but currently
		   (cwdaemon 0.11.0, 0.12.0) this is not the case: the order of
		   events is reverse. Right now I'm not willing to fix it yet.

		   TODO acerion 2023.12.30: fix the order of the two events in
		   cwdaemon. At the very least decrease the time difference
		   between the events from current ~300ms to few ms.
		*/
		test_log_warn("Expectation %d: unexpected order of events: Morse -> socket\n", expectation_idx);
		//return -1; /* TODO acerion 2024.01.26: uncomment after your are certain of correct order of events. */
	} else {
		/* This is the current incorrect behaviour that is accepted for
		   now. */
		test_log_warn("Expectation %d: incorrect (but currently expected) order of events: socket -> Morse\n", expectation_idx);
	}

	/* Always return zero because I don't know what is the truly correct result. */
	return 0;
}




int expect_count_of_events(int expectation_idx, int n_recorded, int n_expected)
{
	if (n_recorded != n_expected) {
		test_log_err("Expectation %d: unexpected count of events: recorded %d events, expected %d events\n",
		             expectation_idx, n_recorded, n_expected);
		return -1;
	}
	test_log_info("Expectation %d: found expected count of events: %d\n", expectation_idx, n_recorded);

	return 0;
}


