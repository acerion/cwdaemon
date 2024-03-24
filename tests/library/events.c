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
   Functions and data structures for events, used in for cwdaemon tests.

   Events encapsulate actions performed by cwdaemon server that reflect
   functionalities of cwdaemon.

   The actions can include:
    - keying a Morse code on cwdevice,
    - toggling PTT pin on cwdevice,
    - sending back reply to server's network client.

   Recording the actions and confirming that they occur in proper order and
   at proper interval is vital for verification and validation of cwdaemon's
   behaviour.
*/




#define _POSIX_C_SOURCE 200809L /* struct timespec */




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




void events_print(const events_t * events)
{
	const struct timespec first_ts = events->events[0].tstamp;


	const size_t n_events = sizeof (events->events) / sizeof (events->events[0]);
	for (size_t e = 0; e < n_events; e++) {

		const event_t * event = &events->events[e];

		if (event->event_type == event_type_none) {
			/* End of events. */
			break;
		}

		/* Normalize timestamps. Make the timestamps more readable. */
		const struct timespec ts = event->tstamp;
		struct timespec diff = { 0 };
		timespec_diff(&first_ts, &ts, &diff);

		switch (event->event_type) {
		case event_type_morse_receive:
			test_log_debug("Test: event #%02zu: %3ld.%09ld: Morse receive:  [%s]\n",
			               e,
			               diff.tv_sec, diff.tv_nsec,
			               event->u.morse_receive.string);
			break;
		case event_type_socket_receive:
			{
				char printable[PRINTABLE_BUFFER_SIZE(sizeof (event->u.socket_receive.bytes))] = { 0 };
				test_log_debug("Test: event #%02zu: %3ld.%09ld: socket receive: [%s]\n",
				               e,
				               diff.tv_sec, diff.tv_nsec,
				               get_printable_string(event->u.socket_receive.bytes, printable, sizeof (printable)));
			}
			break;
		case event_type_sigchld:
			test_log_debug("Test: event #%02zu: %3ld.%09ld: SIGCHLD: wstatus = 0x%04x\n",
			               e,
			               diff.tv_sec, diff.tv_nsec,
			               (unsigned int) event->u.sigchld.wstatus);
			break;
		case event_type_request_exit:
			test_log_debug("Test: event #%02zu: %3ld.%09ld: EXIT request\n",
			               e,
			               diff.tv_sec, diff.tv_nsec);
			break;
		case event_type_none:
		default:
			break;
		}
	}
}




void events_clear(events_t * events)
{
	memset(events->events, 0, sizeof (events->events));
	events->event_idx = 0;

	/* Don't touch events::mutex. */

}




int events_insert_morse_receive_event(events_t * events, const char * buffer, struct timespec * last_character_receive_tstamp)
{
	pthread_mutex_lock(&events->mutex);
	{
		event_t * event = &events->events[events->event_idx];
		event->event_type = event_type_morse_receive;
		event->tstamp = *last_character_receive_tstamp;

		event_morse_receive_t * morse = &event->u.morse_receive;
		const size_t n = sizeof (morse->string);
		strncpy(morse->string, buffer, n);
		morse->string[n - 1] = '\0';

		events->event_idx++;
	}
	pthread_mutex_unlock(&events->mutex);

	return 0;
}




int events_insert_socket_receive_event(events_t * events, const socket_receive_data_t * received)
{
	struct timespec spec = { 0 };
	clock_gettime(CLOCK_MONOTONIC, &spec);

	pthread_mutex_lock(&events->mutex);
	{
		event_t * event = &events->events[events->event_idx];
		event->event_type = event_type_socket_receive;
		event->tstamp = spec;

		memcpy(&event->u.socket_receive, received, sizeof (socket_receive_data_t));

		events->event_idx++;
	}
	pthread_mutex_unlock(&events->mutex);

	return 0;
}




int events_insert_sigchld_event(events_t * events, const child_exit_info_t * exit_info)
{
	pthread_mutex_lock(&events->mutex);
	{
		events->events[events->event_idx].tstamp = exit_info->sigchld_timestamp;
		events->events[events->event_idx].event_type = event_type_sigchld;
		events->events[events->event_idx].u.sigchld.wstatus = exit_info->wstatus;

		events->event_idx++;
		//qsort(events->events, events->event_idx, sizeof (event_t), event_sort_fn);
	}
	pthread_mutex_unlock(&events->mutex);

	return 0;
}



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

	qsort(events->events, (size_t) events->event_idx, sizeof (event_t), cmpevent);

	if (do_debug) {
		test_log_debug("Test: events after sort: %s\n", "");
		test_log_debug("Test: vvvvvvvv %s\n", "");
		events_print(events);
		test_log_debug("Test: ^^^^^^^^ %s\n", "");
	}

	return 0;
}




int events_find_by_type(const events_t * events, event_type_t type, int * first_idx)
{
	int found_cnt = 0;
	for (int i = 0; i < events->event_idx; i++) {
		if (events->events[i].event_type == type) {
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
		if (events[i].event_type == event_type_none) {
			break;
		}
	}

	return i;
}

