/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   @file

   Functions and data structures for events, used in for cwdaemon tests.

   Events encapsulate actions performed either by tests or by cwdaemon server
   that reflect functionalities of cwdaemon.

   The actions include:
    - keying a Morse code on cwdevice by cwdaemon,
    - toggling PTT pin on cwdevice by cwdaemon,
    - sending back reply to tests by cwdaemon,
    - sending EXIT Escape request to cwdaemon by tests,
    - receiving SIGCHLD signal from Operating System when tested cwdaemon server exits.

   Recording the actions and confirming that they occur in proper order and
   at proper intervals is vital for verification and validation of cwdaemon's
   behaviour.
*/




#define _POSIX_C_SOURCE 200809L /* struct timespec */




#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tests/library/events.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/string_utils.h"
#include "tests/library/time_utils.h"




void events_print(events_t const * events)
{
	struct timespec const * first_ts = &events->events[0].tstamp;


	const size_t n_events = sizeof (events->events) / sizeof (events->events[0]);
	for (size_t idx = 0; idx < n_events; idx++) {

		const event_t * event = &events->events[idx];

		if (event->etype == etype_none) {
			/* End of events. */
			break;
		}

		// All timestamps will be relative to timestamp of first event to
		// make them more readable (to make time diffs between events easier
		// to recognize and read).
		struct timespec const * this_ts = &event->tstamp;
		struct timespec relative_ts = { 0 };
		timespec_diff(first_ts, this_ts, &relative_ts);

		switch (event->etype) {
		case etype_morse:
			test_log_debug("Test: event #%02zu: %3ld.%09ld: received Morse: [%s]\n",
			               idx,
			               relative_ts.tv_sec, relative_ts.tv_nsec,
			               event->u.morse.string);
			break;
		case etype_reply:
			{
				char printable[PRINTABLE_BUFFER_SIZE(sizeof (event->u.reply.bytes))] = { 0 };
				test_log_debug("Test: event #%02zu: %3ld.%09ld: received reply: [%s]\n",
				               idx,
				               relative_ts.tv_sec, relative_ts.tv_nsec,
				               get_printable_string(event->u.reply.bytes, event->u.reply.n_bytes, printable, sizeof (printable)));
			}
			break;
		case etype_sigchld:
			test_log_debug("Test: event #%02zu: %3ld.%09ld: received SIGCHLD: wstatus = 0x%04x\n",
			               idx,
			               relative_ts.tv_sec, relative_ts.tv_nsec,
			               (unsigned int) event->u.sigchld.wstatus);
			break;
		case etype_req_exit:
			test_log_debug("Test: event #%02zu: %3ld.%09ld: sent EXIT request\n",
			               idx,
			               relative_ts.tv_sec, relative_ts.tv_nsec);
			break;
		case etype_none:
		default:
			break;
		}
	}
}




void events_clear(events_t * events)
{
	pthread_mutex_lock(&events->mutex);
	{
		memset(events->events, 0, sizeof (events->events));
		events->events_cnt = 0;
	}
	pthread_mutex_unlock(&events->mutex);

	/* Don't clear events::mutex in any way. */
}




// @reviewed_on{2024.06.02}
int events_insert_morse_receive_event(events_t * events, const char * buffer, struct timespec * last_character_receive_tstamp)
{
	pthread_mutex_lock(&events->mutex);
	{
		if (events->events_cnt >= EVENTS_MAX) {
			test_log_err("Test: trying to record too many events (Morse receive): current count of stored events = %d, limit = %d\n", events->events_cnt, EVENTS_MAX);
			assert(0);
		} else {
			event_t * event = &events->events[events->events_cnt];
			event->etype = etype_morse;
			event->tstamp = *last_character_receive_tstamp;

			// TODO (acerion) 2024.04.18: add some error checking if incoming
			// message is no longer than available space.
			event_morse_receive_t * morse = &event->u.morse;
			const size_t n = sizeof (morse->string);
			strncpy(morse->string, buffer, n);
			morse->string[n - 1] = '\0';

			events->events_cnt++;
		}
	}
	pthread_mutex_unlock(&events->mutex);

	return 0;
}




// @reviewed_on{2024.06.02}
int events_insert_reply_received_event(events_t * events, const test_reply_data_t * received)
{
	pthread_mutex_lock(&events->mutex);
	{
		if (events->events_cnt >= EVENTS_MAX) {
			test_log_err("Test: trying to record too many events (reply receive): current count of stored events = %d, limit = %d\n", events->events_cnt, EVENTS_MAX);
			assert(0);
		} else {
			struct timespec timestamp = { 0 };
			clock_gettime(CLOCK_MONOTONIC, &timestamp);

			event_t * event = &events->events[events->events_cnt];
			event->etype = etype_reply;
			event->tstamp = timestamp;

			memcpy(&event->u.reply, received, sizeof (test_reply_data_t));

			events->events_cnt++;
		}
	}
	pthread_mutex_unlock(&events->mutex);

	return 0;
}




// @reviewed_on{2024.06.02}
int events_insert_sigchld_event(events_t * events, const child_exit_info_t * exit_info)
{
	pthread_mutex_lock(&events->mutex);
	{
		if (events->events_cnt >= EVENTS_MAX) {
			test_log_err("Test: trying to record too many events (sigchld): current count of stored events = %d, limit = %d\n", events->events_cnt, EVENTS_MAX);
			assert(0);
		} else {
			events->events[events->events_cnt].tstamp = exit_info->sigchld_timestamp;
			events->events[events->events_cnt].etype = etype_sigchld;
			events->events[events->events_cnt].u.sigchld.wstatus = exit_info->wstatus;

			events->events_cnt++;
			//qsort(events->events, events->events_cnt, sizeof (event_t), event_sort_fn);
		}
	}
	pthread_mutex_unlock(&events->mutex);

	return 0;
}




// @reviewed_on{2024.06.02}
int events_insert_exit_escape_request_event(events_t * events)
{
	pthread_mutex_lock(&events->mutex);
	{
		if (events->events_cnt >= EVENTS_MAX) {
			test_log_err("Test: trying to record too many events (EXIT Escape request): current count of stored events = %d, limit = %d\n", events->events_cnt, EVENTS_MAX);
			assert(0);
		} else {
			clock_gettime(CLOCK_MONOTONIC, &events->events[events->events_cnt].tstamp);
			events->events[events->events_cnt].etype = etype_req_exit;
			events->events_cnt++;
		}
	}
	pthread_mutex_unlock(&events->mutex);

	return 0;
}




/// @brief Compare two events by their timestamp
///
/// Function used as "compar" argument to qsort().
///
/// @reviewed_on{2024.04.18}
///
/// @return value expected from "compar" function
static int cmpevent(const void * p1, const void * p2)
{
	const event_t * a = (const event_t *) (const long int *) p1;
	const event_t * b = (const event_t *) (const long int *) p2;

	if (a->tstamp.tv_sec < b->tstamp.tv_sec) {
		return -1;
	} else if (a->tstamp.tv_sec > b->tstamp.tv_sec) {
		return 1;
	} else {
		if (a->tstamp.tv_nsec < b->tstamp.tv_nsec) {
			return -1;
		} else if (a->tstamp.tv_nsec > b->tstamp.tv_nsec) {
			return 1;
		} else {
			return 0;
		}
	}
}




int events_sort(events_t * events)
{
	const bool do_debug = false;
	if (do_debug) {
		test_log_debug("Test: events before sort: %s\n", "");
		test_log_debug("Test: vvvvvvvv %s\n", "");
		events_print(events);
		test_log_debug("Test: ^^^^^^^^ %s\n", "");
	}

	qsort(events->events, (size_t) events->events_cnt, sizeof (event_t), cmpevent);

	if (do_debug) {
		test_log_debug("Test: events after sort: %s\n", "");
		test_log_debug("Test: vvvvvvvv %s\n", "");
		events_print(events);
		test_log_debug("Test: ^^^^^^^^ %s\n", "");
	}

	return 0;
}




int events_find_by_type(events_t const * events, event_type_t type, int * first_idx)
{
	int found_cnt = 0;
	for (int i = 0; i < events->events_cnt; i++) {
		if (events->events[i].etype == type) {
			found_cnt++;
			if (1 == found_cnt) {
				*first_idx = i;
			}
		}
	}

	return found_cnt;
}




int events_get_count(const event_t events[EVENTS_MAX])
{
	int i = 0;
	for (i = 0; i < EVENTS_MAX; i++) {
		if (events[i].etype == etype_none) {
			break;
		}
	}

	return i;
}

