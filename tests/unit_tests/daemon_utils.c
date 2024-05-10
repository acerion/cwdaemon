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
/// Unit tests for cwdaemon/src/utils.c.




#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#include "src/utils.h"
#include "tests/library/log.h"




static int test_build_full_device_path_success(void);
static int test_build_full_device_path_failure(void);
static int test_build_full_device_path_length(void);
static int test_find_opt_value(void);
static int test_cwdaemon_get_long(void);




static int (*tests[])(void) = {
	test_build_full_device_path_success,
	test_build_full_device_path_failure,
	test_build_full_device_path_length,
	test_find_opt_value,
	test_cwdaemon_get_long,
	NULL
};




int main(void)
{
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




/// @brief Testing different success cases.
///
/// @reviewed_on{2024.04.23}
static int test_build_full_device_path_success(void)
{
	// All these cases are valid cases. Tested function should succeed in
	// building *some* path. The path may represent a non-existing device,
	// but it will always be a valid string starting with "/dev/".
	const struct {
		const char * input;
		int expected_retv;
		const char * expected_path;
	} test_data[] = {
		{ "/dev/ttyUSB0",            0, "/dev/ttyUSB0"       },
		{ "dev/ttyUSB0",             0, "/dev/dev/ttyUSB0"   },
		{ "ttyS0",                   0, "/dev/ttyS0"         },
		{ "/ttyS0",                  0, "/dev//ttyS0"        }, // The tested function doesn't canonicalize result, hence "//".
		{ "../..//ttyS0",            0, "/dev/../..//ttyS0"  }, // The tested function doesn't canonicalize result, hence relative path components.
	};


	for (size_t i = 0; i < sizeof (test_data) / sizeof (test_data[0]); i++) {
		char result[MAXPATHLEN] = { 0 };
		const int retv = build_full_device_path(result, sizeof (result), test_data[i].input);
		if (retv != test_data[i].expected_retv) {
			test_log_err("build_full_device_path(%s, %zu, %s) gives wrong return value %d/%s (success test #%zu)\n",
			             result, sizeof (result), test_data[i].input, retv, strerror(-retv), i);
			return -1;
		}

		if (0 != strcmp(result, test_data[i].expected_path)) {
			test_log_err("build_full_device_path(%s, %zu, %s) gives wrong result [%s] (success test #%zu)\n",
			             result, sizeof (result), test_data[i].input, result, i);
			return -1;
		}
	}

	return 0;
}




/// @brief Testing different failure cases.
///
/// @reviewed_on{2024.04.23}
static int test_build_full_device_path_failure(void)
{
	char small_buffer[4]; /* Buffer too small to fit a result. */
	char big_buffer[20];  /* Buffer that is big enough to fit a result. Failure will be caused by something other than size of output buffer */
	const struct {
		char * result;
		size_t result_size;
		const char * input;
		int expected_retv;
	} test_data[] = {
		{ small_buffer, sizeof (small_buffer),   "/dev/tty0",  -ENAMETOOLONG },  /* Input is a too long device path. */
		{ small_buffer, sizeof (small_buffer),   "tty0",       -ENAMETOOLONG },  /* Input is a too long device name. */

		{ NULL,         sizeof (big_buffer),     "tty0",       -EINVAL       },  /* Invalid 'result' arg. */
		{ big_buffer,   0,                       "tty0",       -EINVAL       },  /* Invalid 'size' arg. */
		{ big_buffer,   sizeof (big_buffer),     NULL,         -EINVAL       },  /* Invalid 'input' arg. */
	};

	for (size_t i = 0; i < sizeof (test_data) / sizeof (test_data[0]); i++) {
		int retv = build_full_device_path(test_data[i].result, test_data[i].result_size, test_data[i].input);
		if (test_data[i].expected_retv != retv) {
			test_log_err("build_full_device_path(%s, %zu, %s) gives wrong return value %d/%s (failure test #%zu)\n",
			             test_data[i].result, test_data[i].result_size, test_data[i].input, retv, strerror(-retv), i);
			return -1;
		}
	}

	return 0;
}




/// Tests designed specifically to check for correct handling of inputs of
/// different lengths.
///
/// @reviewed_on{2024.04.23}
static int test_build_full_device_path_length(void)
{
	// Buffer large enough to store "/dev/null", but to truncate
	// "/dev/null2".
	char a_buffer[10];

	const struct {
		char * result;
		size_t result_size;
		const char * input;
		int expected_retv;
		const char * expected_result;
	} test_data[] = {
		{ a_buffer, sizeof (a_buffer),   "/dev/null",  0,             "/dev/null" },  /* Input path has 9 characters, it will fit into a_buffer. */
		{ a_buffer, sizeof (a_buffer),   "null",       0,             "/dev/null" },  /* Input name + added prefix will give 9 characters, it will fit into a_buffer. */

		{ a_buffer, sizeof (a_buffer),   "/dev/null2", -ENAMETOOLONG, "/dev/null" },  /* Input path has 10 characters, it will NOT fit into a_buffer - it will be truncated. */
		{ a_buffer, sizeof (a_buffer),   "null3",      -ENAMETOOLONG, "/dev/null" },  /* Input name + added prefix will give 10 characters, it will NOT fit into a_buffer - it will be truncated. */
	};

	for (size_t i = 0; i < sizeof (test_data) / sizeof (test_data[0]); i++) {
		memset(a_buffer, 0, sizeof (a_buffer));

		const int retv = build_full_device_path(test_data[i].result, test_data[i].result_size, test_data[i].input);
		if (retv != test_data[i].expected_retv) {
			test_log_err("build_full_device_path(%s, %zu, %s) gives wrong return value %d/%s (length test #%zu)\n",
			             test_data[i].result, test_data[i].result_size, test_data[i].input, retv, strerror(-retv), i);
			return -1;
		}
		if (0 != strcmp(test_data[i].result, test_data[i].expected_result)) {
			test_log_err("build_full_device_path(%s, %zu, %s) gives wrong result [%s] (length test #%zu)\n",
			             test_data[i].result, test_data[i].result_size, test_data[i].input, test_data[i].result, i);
			return -1;
		}
	}

	return 0;
}




/// @brief This function tests both success and failure cases
///
/// @reviewed_on{2024.04.23}
static int test_find_opt_value(void)
{
	const struct {
		const char * input;
		const char * searched_key;
		opt_t expected_retv;
		const char * expected_value;
	} test_data[] = {
		/* Success cases. */
		{ "ptt=none",         "ptt",          opt_success,  "none"   }, /* Basic case. */
		{ "day=monday",       "day",          opt_success,  "monday" }, /* Basic case. */
		{ "Ptt=none",         "ptt",          opt_success,  "none"   }, /* Test for case-insensitive-ness. */
		{ "day=monday",       "DAY",          opt_success,  "monday" }, /* Test for case-insensitive-ness. */
		{ "q=a",              "q",            opt_success,  "a"      }, /* Short key string. */
		{ "empty=",           "empty",        opt_success,  ""       }, /* Empty value string. */


		/* Failure cases. */
		{ "pt=none",          "ptt",          opt_key_not_found,    NULL }, /* Initial implementation in ttys.c somehow was able to find "ptt" key in "pt=none". */
		{ "ptt=none",         "pt",           opt_key_not_found,    NULL }, /* Opposite of above: searched key is shorter than key in input string. */
		{ "=none",            "pt",           opt_key_not_found,    NULL },

		{ "ptnone",           "ptt",          opt_eq_not_found,     NULL },
		{ "ptt-none",         "ptt",          opt_eq_not_found,     NULL },
		{ "ptt none",         "ptt",          opt_eq_not_found,     NULL },
		{ "ptt",              "ptt",          opt_eq_not_found,     NULL },

		{ "ptt =none",        "ptt",          opt_extra_spaces,     NULL },
		{ "ptt= none",        "ptt",          opt_extra_spaces,     NULL },
		{ "ptt = none",       "ptt",          opt_extra_spaces,     NULL },
	};

	for (size_t i = 0; i < sizeof (test_data) / sizeof (test_data[0]); i++) {
		const char * value = NULL;
		const opt_t retv = find_opt_value(test_data[i].input, test_data[i].searched_key, &value);
		if (retv != test_data[i].expected_retv) {
			test_log_err("find_opt_value(%s, %s, ...) returns unexpected retv: retv = %u, expected = %u in test #%zu\n",
			             test_data[i].input, test_data[i].searched_key,
			             retv, test_data[i].expected_retv, i);
			return -1;
		}
		if (retv == opt_success) {
			if (0 != strcmp(value, test_data[i].expected_value)) {
				test_log_err("find_opt_value(%s, %s, ...) returns unexpected value: value = [%s], expected = [%s] in test #%zu\n",
				             test_data[i].input, test_data[i].searched_key,
				             value, test_data[i].expected_value, i);
				return -1;
			}
		}
	}

	return 0;
}




/// @reviewed_on{2024.04.23}
static int test_cwdaemon_get_long(void)
{
	const long doesnt_matter = 6789;

	const struct test_case_t {
		const char * input;
		bool expected_retv;
		long int expected_value;
	} test_cases[] = {
		/* Failure cases. */
		{ "",                     false, doesnt_matter },
#if LONG_MAX == 9223372036854775807L
		{ "-9223372036854775809", false, doesnt_matter }, /* Underflow of long int. */
		{  "9223372036854775808", false, doesnt_matter }, /* Overflow of long int. */
#else /* 32-bit machine, LONG_MAX == 2147483647L */
		{ "-2147483649",          false, doesnt_matter }, /* Underflow of long int. */
		{  "2147483648",          false, doesnt_matter }, /* Overflow of long int. */
#endif
		{ "0x05",                 false, doesnt_matter }, /* Non-decimal notation. */
		{ "4e5",                  false, doesnt_matter }, /* Non-decimal notation. */
		{ "74Morse",              false, doesnt_matter }, /* Characters in the front are decimal digits, but the rest of characters aren't digits. */
		{ "74ac45",               false, doesnt_matter }, /* Characters in the front are decimal digits, but the rest of characters aren't decimal digits. */
		{ "four",                 false, doesnt_matter }, /* None of characters are decimal digits. */
		{ "\03345",               false, doesnt_matter }, /* Leading non-decimal-digit, non-space character. 033(oct) = 27(dec) = 0x1b = ESC. */


		/* Success cases. */
		// TODO (acerion) 2024.04.23: add more tests of really long values
		// for 64-bit platforms.
		{ "-2147483648",          true,  INT_MIN       }, /* INT_MIN, should be handled correctly by function that converts long values. */
		{ "-01024",               true,  -1024         },
		{ "-1024",                true,  -1024         },
		{ "-01",                  true,  -1            },
		{ "-1",                   true,  -1            },
		{ "0",                    true,  0             },
		{ "000",                  true,  0             },
		{ "1024",                 true,  1024          },
		{ "01024",                true,  1024          },
		{ "2147483647",           true,  INT_MAX       }, /* INT_MAX, should be handled correctly by function that converts long values. */
	};


	const size_t n_test_cases = sizeof (test_cases) / sizeof (test_cases[0]);
	for (size_t i = 0; i < n_test_cases; i++) {
		long value = 0;
		const struct test_case_t * tcase = &test_cases[i];

		const bool retv = cwdaemon_get_long(tcase->input, &value);
		if (retv != tcase->expected_retv) {
			test_log_err("Unexpected return value in test case %zu / %zu: %d\n",
			             i + 1, n_test_cases, retv);
			return -1;
		}

		if (retv == false) {
			/* cwdaemon_get_long() failed to convert input string. No need to
			   make further checks. */
			continue;
		}

		if (value != tcase->expected_value) {
			test_log_err("Unexpected converted value in test case %zu / %zu: %ld (expected %ld)\n",
			             i + 1, n_test_cases, value, tcase->expected_value);
			return -1;
		}
	}

	test_log_info("Tests of cwdaemon_get_long() have succeeded %s\n", "");

	return 0;
}

