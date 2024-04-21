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





/**
   @file Unit tests for cwdaemon/src/sleep.c
*/


#define _POSIX_C_SOURCE 199309L


#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <time.h>

#include "src/sleep.h"




static int test_millisleep_nonintr(void);
static void timespec_diff(const struct timespec * first, const struct timespec * second, struct timespec * diff);




static int (*g_tests[])(void) = {
	test_millisleep_nonintr,
	NULL
};




int main(void)
{
	int i = 0;
	while (g_tests[i]) {
		if (0 != g_tests[i]()) {
			fprintf(stdout, "Test result: failure in tests #%d\n", i);
			return -1;
		}
		i++;
	}

	fprintf(stdout, "Test result: success\n");
	return 0;
}




static int test_millisleep_nonintr(void)
{
	const struct {
		unsigned int intended_duration_ms; /* Intended duration of sleep. */
		int expected_retv;
	} test_data[] = {
		{ .intended_duration_ms =  20, .expected_retv = 0 },
		{ .intended_duration_ms =  40, .expected_retv = 0 },
		{ .intended_duration_ms =  80, .expected_retv = 0 },
		{ .intended_duration_ms = 160, .expected_retv = 0 },
		{ .intended_duration_ms = 320, .expected_retv = 0 },
		{ .intended_duration_ms = 640, .expected_retv = 0 },
		{ .intended_duration_ms = 900, .expected_retv = 0 },
	};


	for (size_t i = 0; i < sizeof (test_data) / sizeof (test_data[0]); i++) {

		struct timespec start = { 0 };
		clock_gettime(CLOCK_MONOTONIC, &start);

		int retv = millisleep_nonintr(test_data[i].intended_duration_ms);
		if (retv != test_data[i].expected_retv) {
			fprintf(stderr, "[EE] millisleep_nonintr(%u): wrong return value: got %d, expected %d in test %zu\n",
			        test_data[i].intended_duration_ms, retv, test_data[i].expected_retv, i);
			return -1;
		}

		struct timespec stop = { 0 };
		clock_gettime(CLOCK_MONOTONIC, &stop);

		struct timespec duration = { 0 };
		timespec_diff(&start, &stop, &duration);

		if (duration.tv_sec > 0) {
			/* TODO acerion 2024.03.27: add tests that can test sleep longer than second. */
			fprintf(stderr, "[EE] unexpectedly slept for a second or more in test %zu\n", i);
			return -1;
		}

		const long int lower_ms = test_data[i].intended_duration_ms - (long int) (test_data[i].intended_duration_ms * 0.05);
		const long int upper_ms = test_data[i].intended_duration_ms + (long int) (test_data[i].intended_duration_ms * 0.05);

		const long int actual_duration_ms = duration.tv_nsec / CWDAEMON_NANOSECS_PER_MILLISEC;
		if (actual_duration_ms < lower_ms) {
			fprintf(stderr, "[EE] duration of sleep is shorter than expected: slept %ld, expected to sleep %ld in test %zu\n",
			        actual_duration_ms, lower_ms, i);
			return -1;
		}
		if (actual_duration_ms > upper_ms) {
			fprintf(stderr, "[EE] duration of sleep is longer than expected: slept %ld, expected to sleep %ld in test %zu\n",
			        actual_duration_ms, upper_ms, i);
			return -1;
		}
	}

	return 0;
}




/// @brief Get difference between two time stamps
///
/// This function is copied from tests/library/time_utils.c.
///
/// Get difference between an earlier timestamp @p first and later timestamp
/// @p second. Put the difference in @p diff.
///
/// Caller must make sure that @p first occurred before @p second, otherwise
/// the result will be incorrect.
///
/// @param[in] first First (earlier) timestamp
/// @param[in] second Second (later) timestamp
/// @param[out] diff The difference between @p first and @p second
static void timespec_diff(const struct timespec * first, const struct timespec * second, struct timespec * diff)
{
	diff->tv_sec  = second->tv_sec - first->tv_sec;
	diff->tv_nsec = second->tv_nsec - first->tv_nsec;

	if (diff->tv_nsec < 0) {
		diff->tv_sec--;
		diff->tv_nsec += CWDAEMON_NANOSECS_PER_SEC;
	}
}

