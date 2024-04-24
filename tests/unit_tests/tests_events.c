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




/// @file
///
/// Unit tests for tests/library/events.c.




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
			test_log_err("Test result: FAIL in test #%d\n", i);
			return -1;
		}
		i++;
	}

	test_log_info("Test result: PASS %s\n", "");
	return 0;
}




/// @reviewed_on{2024.04.24}
static int test_events_sort(void)
{
	// Events to be sorted. Their time stamps are not in order.
	events_t events = {
		.events = {
			{ .etype = etype_morse,      .tstamp = { .tv_sec = 1, .tv_nsec = 5 }, .u.morse_receive = { .string = "Five" }, },
			{ .etype = etype_reply,      .tstamp = { .tv_sec = 5, .tv_nsec = 5 }, .u.reply = { .n_bytes = 4, .bytes = "Four" }, },
			{ .etype = etype_morse,      .tstamp = { .tv_sec = 1, .tv_nsec = 1 }, .u.morse_receive = { .string = "One" }, },
			{ .etype = etype_sigchld,    .tstamp = { .tv_sec = 2, .tv_nsec = 4 }, .u.sigchld = { .wstatus = 7 }, },
			{ .etype = etype_sigchld,    .tstamp = { .tv_sec = 1, .tv_nsec = 4 }, .u.sigchld = { .wstatus = 3 }, },
		},
		.events_cnt = 5,
		.mutex = PTHREAD_MUTEX_INITIALIZER,
	};

	// This is how events sorted by time stamp look like.
	const events_t expected = {
		.events = {
			{ .etype = etype_morse,      .tstamp = { .tv_sec = 1, .tv_nsec = 1 }, .u.morse_receive = { .string = "One" }, },
			{ .etype = etype_sigchld,    .tstamp = { .tv_sec = 1, .tv_nsec = 4 }, .u.sigchld = { .wstatus = 3 }, },
			{ .etype = etype_morse,      .tstamp = { .tv_sec = 1, .tv_nsec = 5 }, .u.morse_receive = { .string = "Five" }, },
			{ .etype = etype_sigchld,    .tstamp = { .tv_sec = 2, .tv_nsec = 4 }, .u.sigchld = { .wstatus = 7 }, },
			{ .etype = etype_reply,      .tstamp = { .tv_sec = 5, .tv_nsec = 5 }, .u.reply = { .n_bytes = 4, .bytes = "Four" }, },
		},
		.events_cnt = 5,
		.mutex = PTHREAD_MUTEX_INITIALIZER,
	};

	// Function under test.
	const int retv = events_sort(&events);
	if (0 != retv) {
		test_log_err("Unit tests: events_sort() returns non-success: %d\n", retv);
		return -1;
	}

	// Comparing all members one by one instead of using memcmp() is due to
	// the fact that clang-tidy complains about usage of memcmp() in this
	// context:
	// ./tests_events.c:109:11: warning: comparing object representation of type 'events_t' which does not have a unique object representation; consider comparing the members of the object manually [bugprone-suspicious-memory-comparison,cert-exp42-c,cert-flp37-c]
	for (int i = 0; i < expected.events_cnt; i++) {
		if (expected.events[i].etype != events.events[i].etype) {
			test_log_err("Unit tests: events_sort() failed at event type in event %d\n", i);
			return -1;
		}
		if (expected.events[i].tstamp.tv_sec != events.events[i].tstamp.tv_sec
		    || expected.events[i].tstamp.tv_nsec != events.events[i].tstamp.tv_nsec) {
			test_log_err("Unit tests: events_sort() failed at event timestamp in event %d\n", i);
			return -1;
		}

		switch (expected.events[i].etype) {
		case etype_morse:
			if (0 != memcmp(&expected.events[i].u.morse_receive, &events.events[i].u.morse_receive, sizeof (event_morse_receive_t))) {
				test_log_err("Unit tests: events_sort() failed at 'morse receive' member in event %d\n", i);
				return -1;
			}
			break;
		case etype_reply:
			/* TODO: clang complained about usage of memcmp() for entire struct, so I have to now be careful to compare all members and never miss some member. */
			if (0 != memcmp(&expected.events[i].u.reply.bytes, &events.events[i].u.reply.bytes, sizeof (events.events[i].u.reply.bytes))) {
				test_log_err("Unit tests: events_sort() failed at 'reply.bytes' member in event %d\n", i);
				return -1;
			}
			if (expected.events[i].u.reply.n_bytes != events.events[i].u.reply.n_bytes) {
				test_log_err("Unit tests: events_sort() failed at 'reply.n_bytes' member in event %d\n", i);
				return -1;
			}
			break;
		case etype_req_exit:
			/* TODO: acerion 2024.01.28: If you add "exit" member to union,
			   make sure to update this part of code. */
			break;
		case etype_sigchld:
			if (0 != memcmp(&expected.events[i].u.sigchld, &events.events[i].u.sigchld, sizeof (event_sigchld_t))) {
				test_log_err("Unit tests: events_sort() failed at 'sigchld' member in event %d\n", i);
				return -1;
			}
			break;
		case etype_none:
		default:
			break;
		}
	}

	test_log_info("Unit tests: events_sort() passed test %s\n", "");
	return 0;
}

