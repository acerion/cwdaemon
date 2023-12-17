/*
 * sleep.c - sleep functions for cwdaemon
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




#define _GNU_SOURCE /* struct timespec */




#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h> /* nanosleep() */
#include <unistd.h>

#include "sleep.h"




int microsleep_nonintr(unsigned int usecs)
{
	const unsigned long seconds = usecs / CWDAEMON_MICROSECS_PER_SEC;
	const unsigned long micros  = usecs % CWDAEMON_MICROSECS_PER_SEC;
	struct timespec remaining = {
		.tv_sec  = (long) seconds,
		.tv_nsec = (long) (micros * CWDAEMON_NANOSECS_PER_MICROSEC)
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
				perror("nanosleep()");
				return -1;
			}
		}
	} while (0 != retv);

	return 0;
}




int millisleep_nonintr(unsigned int millisecs)
{
	return microsleep_nonintr(millisecs * CWDAEMON_MICROSECS_PER_MILLISEC);
}




int sleep_nonintr(unsigned int secs)
{
	/*
	  Implementing as direct call to nanosleep() instead of implementing it
	  as a wrapper to microsleep_nonintr() because I want to avoid first
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
				perror("nanosleep()");
				return -1;
			}
		}
	} while (0 != retv);

	return 0;
}

