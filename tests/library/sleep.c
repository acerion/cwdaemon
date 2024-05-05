/*
 * sleep.c - sleep functions for cwdaemon
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

   Sleep functions.

   The same file exists in two places:
    - src/sleep.c
    - tests/library/sleep.c

   The reasoning behind the duplication is following:
   1. I need sleep functions in daemon and in test code.
   2. On one hand I could have single definition of the functions and share
      them between daemon and test code, which would lead to increased
      coupling between the two parts of cwdaemon package.
   3. on the other hand I could have copies of definitions of the function in
      daemon's code and tests' code. This would result in duplication of (small
      amount of) code.
   4. Out of the two options, I now consider the duplication to be lesser evil.

   If I went with having common implementation of sleep functions for daemon
   and for tests, this would make build system configuration a bit more
   complicated.

   TODO acerion 2024.01.07: be aware of the duplication and try to keep the
   files in the two locations in sync.
*/




#define _GNU_SOURCE /* struct timespec */




#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h> /* nanosleep() */
#include <unistd.h>

#include "log.h"
#include "tests/library/sleep.h"
#include "time_utils.h"




int test_microsleep_nonintr(unsigned int usecs)
{
	const unsigned long seconds = usecs / TESTS_MICROSECS_PER_SEC;
	const unsigned long micros  = usecs % TESTS_MICROSECS_PER_SEC;
	struct timespec remaining = {
		.tv_sec  = (long) seconds,
		.tv_nsec = (long) (micros * TESTS_NANOSECS_PER_MICROSEC)
	};

	int retv = 0;
	do {
		struct timespec req = { .tv_sec = remaining.tv_sec, .tv_nsec = remaining.tv_nsec };
		retv = nanosleep(&req, &remaining);
		if (0 != retv) {
			switch (errno) {
			case EINTR:
				break;
			default:
				test_log_err("nanosleep: errno = %d", errno);
				return -1;
			}
		}
	} while (0 != retv);

	return 0;
}




int test_millisleep_nonintr(unsigned int millisecs)
{
	return test_microsleep_nonintr(millisecs * TESTS_MICROSECS_PER_MILLISEC);
}




int test_sleep_nonintr(unsigned int secs)
{
	/*
	  Implementing as direct call to nanosleep() instead of implementing it
	  as a wrapper to test_microsleep_nonintr() because I want to avoid first
	  doing a multiplication of function argument by 10^6 and then dividing
	  argument again back by 10^6.
	*/
	struct timespec remaining = {
		.tv_sec  = secs,
		.tv_nsec = 0
	};

	int retv = 0;
	do {
		struct timespec req = { .tv_sec = remaining.tv_sec, .tv_nsec = remaining.tv_nsec };
		retv = nanosleep(&req, &remaining);
		if (0 != retv) {
			switch (errno) {
			case EINTR:
				break;
			default:
				test_log_err("nanosleep: errno = %d", errno);
				return -1;
			}
		}
	} while (0 != retv);

	return 0;
}

