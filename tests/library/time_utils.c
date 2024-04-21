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




/// @file
///
/// Time functions used in cwdaemon tests.




#define _POSIX_C_SOURCE 200809L /* struct timespec */




#include "tests/library/time_utils.h"




void timespec_diff(const struct timespec * first, const struct timespec * second, struct timespec * diff)
{
	// TODO (acerion) 2024.04.20 add detection of situation where first is
	// not really first. If there is some error in caller, I need to know
	// this.

	diff->tv_sec  = second->tv_sec - first->tv_sec;
	diff->tv_nsec = second->tv_nsec - first->tv_nsec;

	if (diff->tv_nsec < 0) {
		diff->tv_sec--;
		diff->tv_nsec += TESTS_NANOSECS_PER_SEC;
	}
}



