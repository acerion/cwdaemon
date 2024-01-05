/*
 * random.c - randomization functions for cwdaemon
 * Copyright (C) 2023 Kamil Ignacak <acerion@wp.pl>
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




#define _DEFAULT_SOURCE /* random(), srandom() */




#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "random.h"




/**
   Wrappers around libc's random number generator functions.

   Thanks to the wrappers I can easily handle platform-specific stuff in just
   one place. I can also easily swap the functions used to seed and get
   random values.
*/




uint32_t cwdaemon_srandom(uint32_t seed)
{
	if (0 == seed) {
		/* Naive fix for cert-msc32-c. cwdaemon doesn't require anything
		   better now. */

		struct timespec spec;
		clock_gettime(CLOCK_MONOTONIC, &spec);
		/*
		  999_999_999 nanoseconds is represented in hex with 8 hex digits as
		  3b9ac9ff. This means that if I apply 7-digits mask, I will have
		  7x4=28 random-ish bits.
		*/
		const uint32_t nsec = (spec.tv_nsec & 0xfffffff) << (32 - 28);

		const uint32_t local_seed = time(NULL) ^ getpid() ^ nsec;
		srandom((unsigned int) local_seed);
		return local_seed;
	} else {
#ifdef __OpenBSD__
		/*
		  OpenBSD's implementation of srandom() ignores seed, which doesn't
		  allow for *deterministic* pseudorandom sequences. See
		  https://man.openbsd.org/random.3 for more info.

		  I need to have deterministic sequences to be able to repeat a bug
		  that happened when a test was executed with specific seed.

		  cwdaemon doesn't do cryptography, so non-deterministic sequences
		  are not required.
		*/

		srandom_deterministic((unsigned int) seed);
#else
		srandom((unsigned int) seed);
#endif
		return seed;
	}
}




int cwdaemon_random_uint(unsigned int lower, unsigned int upper, unsigned int * result)
{
	unsigned int value = (unsigned int) random();

	value %= ((upper + 1) - lower);
	value += lower;

	*result = value;

	return 0;
}




int cwdaemon_random_bool(bool * result)
{
	const unsigned int lower = 1;
	const unsigned int upper = 100;
	unsigned int uint = 0;
	if (0 != cwdaemon_random_uint(lower, upper, &uint)) {
		return -1;
	}

	*result = uint % 2;
	return 0;
}


