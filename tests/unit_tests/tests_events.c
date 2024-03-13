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
 * GNU Library General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   @file

   Unit tests for tests/library/events.c
*/




#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests/library/events.h"
#include "tests/library/log.h"




static int test_events_sort(void);




static int (*g_tests[])(void) = {
	test_events_sort,
	NULL
};




int main(void)
{
	int i = 0;
	while (g_tests[i]) {
		if (0 != g_tests[i]()) {
			fprintf(stderr, "[EE] Test result: failure in test #%d\n", i);
			return -1;
		}
		i++;
	}

	fprintf(stdout, "[II] Test result: success\n");
	return 0;
}




static int test_events_sort(void)
{
	events_t events = {
		.events = {
			{ .event_type = event_type_morse_receive,         .tstamp = { .tv_sec = 1, .tv_nsec = 5 }, .u.morse_receive = { .string = "Five" }, },
			{ .event_type = event_type_socket_receive,        .tstamp = { .tv_sec = 5, .tv_nsec = 5 }, .u.socket_receive = { .n_bytes = 4, .bytes = "Four" }, },
			{ .event_type = event_type_morse_receive,         .tstamp = { .tv_sec = 1, .tv_nsec = 1 }, .u.morse_receive = { .string = "One" }, },
			{ .event_type = event_type_sigchld,               .tstamp = { .tv_sec = 2, .tv_nsec = 4 }, .u.sigchld = { .wstatus = 7 }, },
			{ .event_type = event_type_sigchld,               .tstamp = { .tv_sec = 1, .tv_nsec = 4 }, .u.sigchld = { .wstatus = 3 }, },
		},
		.event_idx = 5,
		.mutex = PTHREAD_MUTEX_INITIALIZER,
	};

	events_t sorted = {
		.events = {
			{ .event_type = event_type_morse_receive,         .tstamp = { .tv_sec = 1, .tv_nsec = 1 }, .u.morse_receive = { .string = "One" }, },
			{ .event_type = event_type_sigchld,               .tstamp = { .tv_sec = 1, .tv_nsec = 4 }, .u.sigchld = { .wstatus = 3 }, },
			{ .event_type = event_type_morse_receive,         .tstamp = { .tv_sec = 1, .tv_nsec = 5 }, .u.morse_receive = { .string = "Five" }, },
			{ .event_type = event_type_sigchld,               .tstamp = { .tv_sec = 2, .tv_nsec = 4 }, .u.sigchld = { .wstatus = 7 }, },
			{ .event_type = event_type_socket_receive,        .tstamp = { .tv_sec = 5, .tv_nsec = 5 }, .u.socket_receive = { .n_bytes = 4, .bytes = "Four" }, },
		},
		.event_idx = 5,
		.mutex = PTHREAD_MUTEX_INITIALIZER,
	};

	int retv = events_sort(&events);
	if (0 != retv) {
		test_log_err("Unit tests: events_sort() returns non-success: %d\n", retv);
		return -1;
	}

	/*
	  Comparing all members one by one instead of using memcmp() is due to
	  the fact that clang-tidy complains about usage of memcmp() in this
	  context:

	  ./tests_events.c:109:11: warning: comparing object representation of type 'events_t' which does not have a unique object representation; consider comparing the members of the object manually [bugprone-suspicious-memory-comparison,cert-exp42-c,cert-flp37-c]
	*/
	for (int i = 0; i < sorted.event_idx; i++) {
		if (sorted.events[i].event_type != events.events[i].event_type) {
			test_log_err("Unit tests: events_sort() failed at event type in event %d\n", i);
			return -1;
		}
		if (sorted.events[i].tstamp.tv_sec != events.events[i].tstamp.tv_sec
		    || sorted.events[i].tstamp.tv_nsec != events.events[i].tstamp.tv_nsec) {
			test_log_err("Unit tests: events_sort() failed at event timestamp in event %d\n", i);
			return -1;
		}

		switch (sorted.events[i].event_type) {
		case event_type_morse_receive:
			if (0 != memcmp(&sorted.events[i].u.morse_receive, &events.events[i].u.morse_receive, sizeof (event_morse_receive_t))) {
				test_log_err("Unit tests: events_sort() failed at 'morse receive' member in event %d\n", i);
				return -1;
			}
			break;
		case event_type_socket_receive:
			/* TODO: clang complained about usage of memcmp() for entire struct, so I have to now be careful to compare all members and never miss some member. */
			if (0 != memcmp(&sorted.events[i].u.socket_receive.bytes, &events.events[i].u.socket_receive.bytes, sizeof (events.events[i].u.socket_receive.bytes))) {
				test_log_err("Unit tests: events_sort() failed at 'socket_receive.bytes' member in event %d\n", i);
				return -1;
			}
			if (sorted.events[i].u.socket_receive.n_bytes != events.events[i].u.socket_receive.n_bytes) {
				test_log_err("Unit tests: events_sort() failed at 'socket_receive.n_bytes' member in event %d\n", i);
				return -1;
			}
			break;
		case event_type_request_exit:
			/* TODO: acerion 2024.01.28: If you add "exit" member to union,
			   make sure to update this part of code. */
			break;
		case event_type_sigchld:
			if (0 != memcmp(&sorted.events[i].u.sigchld, &events.events[i].u.sigchld, sizeof (event_sigchld_t))) {
				test_log_err("Unit tests: events_sort() failed at 'sigchld' member in event %d\n", i);
				return -1;
			}
			break;
		case event_type_none:
		default:
			break;
		}
	}
	test_log_info("Unit tests: events_sort() passed test %s\n", "");
	return 0;
}

