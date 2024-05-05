/*
 * This file is a part of cwdaemon project.
 *
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
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/// @file
//
/// Unit tests for cwdaemon/tests/library/random.c.




#include <stdio.h>

#include "tests/library/log.h"
#include "tests/library/random.h"




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
	test_log_info("Random seed: 0x%08x (%u)\n", seed, seed);

	int i = 0;
	while (tests[i]) {
		if (0 != tests[i]()) {
			test_log_err("Test result: FAIL in tests #%d\n", i);
			return -1;
		}
		i++;
	}

	test_log_info("Test result: PASS %s\n", "");
	return 0;
}




// Function under test should generate unsigned integers from given range.
static struct test_data_uint {
	unsigned int lower;
	unsigned int upper;
	int expected_retv;
} test_data_uint[] = {
	{ .lower = 0,      .upper = 1000,    .expected_retv = 0 },
	{ .lower = 100,    .upper = 101,     .expected_retv = 0 },
	{ .lower = 1000,   .upper = 2000,    .expected_retv = 0 }
};




/// @reviewed_on{2024.04.24}
static int test_cwdaemon_random_uint(void)
{
	for (size_t test = 0; test < sizeof (test_data_uint) / sizeof (test_data_uint[0]); test++) {

		const struct test_data_uint * tcase = &test_data_uint[test];

		for (unsigned int i = 0; i < CALLS_TO_RANDOM; i++) {
			unsigned int result = 0;
			const int retv = cwdaemon_random_uint(tcase->lower, tcase->upper, &result);
			if (retv != tcase->expected_retv) {
				test_log_err("unexpected return value from cwdaemon_random_uint() for test %zu: %d\n", test, retv);
				return -1;
			}
			if (result < tcase->lower || result > tcase->upper) {
				test_log_err("unexpected result from cwdaemon_random_uint() for test %zu: %u\n", test, result);
				return -1;
			}
#if 0
			test_log_debug("random <%u-%u> = %u\n", tcase->lower, tcase->upper, result);
#endif
		}
	}

	// All generated random values are within specified range.
	test_log_info("Tests of cwdaemon_random_uint() have succeeded %s\n", "");

	return 0;
}




/// Call cwdaemon_random_bool() in a loop. Make sure that the calls returned
/// approximately the same count of 'true' and 'false' values.
///
/// @reviewed_on{2024.04.24}
static int test_cwdaemon_random_bool(void)
{
	// How many 'true' and 'false' values have been generated? For a fair
	// generator of random booleans, at the end of the test these values
	// should be close to each other.
	unsigned int trues = 0;
	unsigned int falses = 0;

	for (unsigned int i = 0; i < CALLS_TO_RANDOM; i++) {
		bool value = false;
		if (0 != cwdaemon_random_bool(&value)) {
			test_log_err("Call #%u to cwdaemon_random_bool() has failed\n", i);
			return -1;
		}
		if (value) {
			trues++;
		} else {
			falses++;
		}
	}

	// This protects from division by zero in next step.
	if (trues == 0 || falses == 0) {
		test_log_err("Either 'trues' or 'falses' counter is zero %s\n", "");
		return -1;
	}

	const double proportion = (double) trues / (double) falses;
	// <0.9 - 1.1> is a pretty wide margin, but these tests aren't a
	// high-security crypto package.
	//
	// In a test that did 20000 calls to this function, the lowest/highest
	// proportion was 0.922/1.076.
	if (proportion < 0.9 || proportion > 1.1) {
		test_log_err("Proportion of trues vs. falses is out of expected range: %.3f\n", proportion);
		return -1;
	}

	test_log_info("Tests of cwdaemon_random_bool() have succeeded (proportion = %.3f)\n", proportion);

	return 0;
}

