/*
 * This file is a part of cwdaemon project.
 *
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

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   @file Unit tests for cwdaemon/src/lib/random.c
*/




#include <stdio.h>

#include "src/lib/random.h"




/* Count of individual calls to random() function per one test parameters
   set. */
#define CALLS_TO_RANDOM 10000




static int test_cwdaemon_random_uint(void);
static int test_cwdaemon_random_bool(void);




static int (*tests[])(void) = {
	test_cwdaemon_random_uint,
	test_cwdaemon_random_bool,
	NULL
};




int main(void)
{
	const uint32_t seed = cwdaemon_srandom(0);
	fprintf(stderr, "[DD] Random seed: 0x%08x (%u)\n", seed, seed);

	int i = 0;
	while (tests[i]) {
		if (0 != tests[i]()) {
			fprintf(stdout, "[EE] Test result: failure in tests #%d\n", i);
			return -1;
		}
		i++;
	}

	fprintf(stdout, "[II] Test result: success\n");
	return 0;
}




static struct test_data_uint {
	unsigned int lower;
	unsigned int upper;
	int expected_retv;
} test_data_uint[] = {
	{ .lower = 0,      .upper = 1000,    .expected_retv = 0 },
	{ .lower = 100,    .upper = 101,     .expected_retv = 0 },
	{ .lower = 1000,   .upper = 2000,    .expected_retv = 0 }
};




static int test_cwdaemon_random_uint(void)
{
	for (size_t test = 0; test < sizeof (test_data_uint) / sizeof (test_data_uint[0]); test++) {

		for (int i = 0; i < CALLS_TO_RANDOM; i++) {
			unsigned int result = 0;
			const int retv = cwdaemon_random_uint(test_data_uint[test].lower,
			                                      test_data_uint[test].upper,
			                                      &result);
			if (retv != test_data_uint[test].expected_retv) {
				fprintf(stderr, "[ERROR] unexpected return value from cwdaemon_random_uint() for test %zu: %d\n", test, retv);
				return -1;
			}
			if (result < test_data_uint[test].lower || result > test_data_uint[test].upper) {
				fprintf(stderr, "[ERROR] unexpected result from cwdaemon_random_uint() for test %zu: %u\n", test, result);
				return -1;
			}
#if 0
			fprintf(stderr, "[DEBUG] random <%u-%u> = %u\n",
			        test_data_uint[test].lower, test_data_uint[test].upper,
			        result);
#endif
		}
	}

	fprintf(stderr, "[II] Tests of cwdaemon_random_uint() have succeeded\n");

	return 0;
}




static int test_cwdaemon_random_bool(void)
{
	int trues = 0;
	int falses = 0;
	for (int i = 0; i < CALLS_TO_RANDOM; i++) {
		bool value = false;
		if (0 != cwdaemon_random_bool(&value)) {
			fprintf(stderr, "[EE] Call #%d to cwdaemon_random_bool() has failed\n", i);
			return -1;
		}
		if (value) {
			trues++;
		} else {
			falses++;
		}
	}

	if (trues == 0 || falses == 0) {
		fprintf(stderr, "[EE] Either 'trues' or 'falses' counter is zero\n");
		return -1;
	}

	const double proportion = (double) trues / (double) falses;
	if (proportion < 0.95 || proportion > 1.05) {
		fprintf(stderr, "[EE] Proportion of trues vs. falses is invalid: %.3f\n", proportion);
		return -1;
	}

	fprintf(stderr, "[II] Tests of cwdaemon_random_bool() have succeeded (proportion = %.3f)\n", proportion);

	return 0;
}

