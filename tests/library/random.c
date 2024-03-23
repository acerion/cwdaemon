/*
 * random.c - randomization functions for cwdaemon
 * Copyright (C) 2023 - 2024 Kamil Ignacak <acerion@wp.pl>
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

#include "log.h"
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
		const uint32_t nsec = (((uint32_t) spec.tv_nsec) & 0xfffffff) << (32 - 28);

		const uint32_t now = (uint32_t) time(NULL);
		const uint32_t pid = (uint32_t) getpid();
		const uint32_t local_seed = now ^ pid ^ nsec;
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

	unsigned int div = ((upper + 1) - lower);
	if (0 == div) {
		/* This may happen if client passes INT_MIN and INT_MAX as lower/upper. */
		test_log_err("Random: trying to divide by zero (calculated from lower = %u, upper = %u)\n", lower, upper);
		return -1;
	}
	value %= div;
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




int cwdaemon_random_biased_bool(unsigned int bias, bool * result)
{
	unsigned int b = 0;
	if (0 != cwdaemon_random_uint(0, bias, &b)) {
		return -1;
	}
	*result = 0 == b;
	return 0;
}




int cwdaemon_random_bytes(char * buffer, size_t size)
{
	for (size_t i = 0; i < size; i++) {
		unsigned int a = 0x00;
		unsigned int b = 0xff;
		unsigned int val = 0;
		if (0 != cwdaemon_random_uint(a, b, &val)) {
			return -1;
		}
		buffer[i] = (char) val;
	}
	return 0;
}

