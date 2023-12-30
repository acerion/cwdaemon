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
 * GNU Library General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */





/**
	@file Unit tests for tests/library/time_utils.c
*/




#define _POSIX_C_SOURCE 200809L /* struct timespec */




#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "tests/library/time_utils.h"




static int test_timespec_diff(void);




static int (*tests[])(void) = {
	test_timespec_diff,
	NULL
};




int main(void)
{
	int i = 0;
	while (tests[i]) {
		if (0 != tests[i]()) {
			fprintf(stdout, "Test result: failure in tests #%d\n", i);
			return -1;
		}
		i++;
	}

	fprintf(stdout, "Test result: success\n");
	return 0;
}




static int test_timespec_diff(void)
{
	/*
	  All these cases are valid cases. Tested function should succeed in
	  building *some* path. The path may represent a non-existing device, but
	  it will always be a valid string starting with "/dev/".
	*/
	const long X = (long) 1000 * 1000; /* Helper constant to make numbers shorter. */
	const struct {
		const struct timespec first;
		const struct timespec second;
		const struct timespec expected;
	} test_data[] = {
		/* TODO acerion 2023.12.30: add tests for cases where second is earlier than first. */
		{ .first = { 0,       0 }, .second = { 0,       0 }, .expected = { 0,       0 } },
		{ .first = { 0, 100 * X }, .second = { 0, 400 * X }, .expected = { 0, 300 * X } },
		{ .first = { 0, 900 * X }, .second = { 1, 200 * X }, .expected = { 0, 300 * X } },
		{ .first = { 0, 900 * X }, .second = { 1, 950 * X }, .expected = { 1,  50 * X } },
		{ .first = { 0, 100 * X }, .second = { 2, 100 * X }, .expected = { 2,   0 * X } },
		{ .first = { 0, 100 * X }, .second = { 2, 900 * X }, .expected = { 2, 800 * X } },
		{ .first = { 2, 100 * X }, .second = { 2, 900 * X }, .expected = { 0, 800 * X } },
		{ .first = { 2, 400 * X }, .second = { 8, 100 * X }, .expected = { 5, 700 * X } },
	};


	const size_t n_tests = sizeof (test_data) / sizeof (test_data[0]);
	for (size_t i = 0; i < n_tests; i++) {
		struct timespec diff = { 0 };
		timespec_diff(&test_data[i].first, &test_data[i].second, &diff);
		if (0 != memcmp(&diff, &test_data[i].expected, sizeof (struct timespec))) {
			fprintf(stderr, "[EE] timespec_diff() gives wrong return value { %ld:%09ld} in test #%zu / %zu\n",
			        diff.tv_sec, diff.tv_nsec,
			        i + 1, n_tests);
			return -1;
		}
	}

	return 0;
}

