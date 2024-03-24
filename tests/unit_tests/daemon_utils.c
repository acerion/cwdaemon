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
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */





/**
   @file Unit tests for cwdaemon/src/utils.c
*/




#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#include "src/utils.h"




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
			fprintf(stdout, "Test result: failure in tests #%d\n", i);
			return -1;
		}
		i++;
	}

	fprintf(stdout, "Test result: success\n");
	return 0;
}




/*
  Testing different success cases.
*/
static int test_build_full_device_path_success(void)
{
	char buffer[MAXPATHLEN] = { 0 };

	/*
	  All these cases are valid cases. Tested function should succeed in
	  building *some* path. The path may represent a non-existing device, but
	  it will always be a valid string starting with "/dev/".
	*/
	const struct {
		const char * input;
		int expected_retv;
		const char * expected_path;
	} test_data[] = {
		{ "/dev/ttyUSB0",            0, "/dev/ttyUSB0"       },
		{ "dev/ttyUSB0",             0, "/dev/dev/ttyUSB0"   },
		{ "ttyS0",                   0, "/dev/ttyS0"         },
		{ "/ttyS0",                  0, "/dev//ttyS0"        },
		{ "../..//ttyS0",            0, "/dev/../..//ttyS0"  },
	};


	for (size_t i = 0; i < sizeof (test_data) / sizeof (test_data[0]); i++) {
		int retv = build_full_device_path(buffer, sizeof (buffer), test_data[i].input);
		if (test_data[i].expected_retv != retv) {
			fprintf(stderr, "[EE] build_full_device_path(%s, %zu, %s) gives wrong return value %d/%s (success test #%zu)\n",
					  buffer, sizeof (buffer), test_data[i].input, retv, strerror(-retv), i);
			return -1;
		}

		if (0 != strcmp(buffer, test_data[i].expected_path)) {
			fprintf(stderr, "[EE] build_full_device_path(%s, %zu, %s) gives wrong result [%s] (success test #%zu)\n",
					  buffer, sizeof (buffer), test_data[i].input, buffer, i);
			return -1;
		}
	}

	return 0;
}




/*
  Testing different failure cases.
*/
static int test_build_full_device_path_failure(void)
{
	char small_buffer[4]; /* Buffer too small to fit a result. */
	char big_buffer[20];  /* Buffer that is big enough to fit a result. */
	const struct {
		const char * input;
		char * buffer;
		size_t buffer_size;
		int expected_retv;
	} test_data[] = {
		{ "/dev/null",  small_buffer, sizeof (small_buffer),   -ENAMETOOLONG },  /* Input is a too long device path. */
		{ "null",       small_buffer, sizeof (small_buffer),   -ENAMETOOLONG },  /* Input is a too long device name. */
		{ NULL,         big_buffer,   sizeof (big_buffer),     -EINVAL       },  /* Invalid 'input' arg. */
		{ "null",       NULL,         sizeof (big_buffer),     -EINVAL       },  /* Invalid 'result' arg. */
		{ "null",       big_buffer,   0,                       -EINVAL       },  /* Invalid 'size' arg. */
	};

	for (size_t i = 0; i < sizeof (test_data) / sizeof (test_data[0]); i++) {
		int retv = build_full_device_path(test_data[i].buffer, test_data[i].buffer_size, test_data[i].input);
		if (test_data[i].expected_retv != retv) {
			fprintf(stderr, "[EE] build_full_device_path(%s, %zu, %s) gives wrong return value %d/%s (failure test #%zu)\n",
					  test_data[i].buffer, test_data[i].buffer_size, test_data[i].input, retv, strerror(-retv), i);
			return -1;
		}
	}

	return 0;
}




/*
  Tests designed specifically to check for correct handling of too long input.
*/
static int test_build_full_device_path_length(void)
{
	char a_buffer[10]; /* If you change size of this buffer, you will need to change test data too. */
	const struct {
		const char * input;
		char * buffer;
		size_t buffer_size;
		int expected_retv;
		const char * expected_result;
	} test_data[] = {
		{ "/dev/null",  a_buffer, sizeof (a_buffer),   0, "/dev/null" },  /* Input path has 9 characters, it will fit into a_buffer. */
		{ "null",       a_buffer, sizeof (a_buffer),   0, "/dev/null" },  /* Input name + added prefix will give 9 characters, it will fit into a_buffer. */

		{ "/dev/null2", a_buffer, sizeof (a_buffer),   -ENAMETOOLONG, "/dev/null" },  /* Input path has 10 characters, it will NOT fit into a_buffer - it will be truncated. */
		{ "null3",      a_buffer, sizeof (a_buffer),   -ENAMETOOLONG, "/dev/null" },  /* Input name + added prefix will give 10 characters, it will NOT fit into a_buffer - it will be truncated. */
	};

	for (size_t i = 0; i < sizeof (test_data) / sizeof (test_data[0]); i++) {
		int retv = build_full_device_path(test_data[i].buffer, test_data[i].buffer_size, test_data[i].input);
		if (test_data[i].expected_retv != retv) {
			fprintf(stderr, "[EE] build_full_device_path(%s, %zu, %s) gives wrong return value %d/%s (length test #%zu)\n",
					  test_data[i].buffer, test_data[i].buffer_size, test_data[i].input, retv, strerror(-retv), i);
			return -1;
		}
		if (0 != strcmp(test_data[i].buffer, test_data[i].expected_result)) {
			fprintf(stderr, "[EE] build_full_device_path(%s, %zu, %s) gives wrong result [%s] (length test #%zu)\n",
					  test_data[i].buffer, test_data[i].buffer_size, test_data[i].input, test_data[i].buffer, i);
			return -1;
		}
	}

	return 0;
}




/* This function tests both success and failure cases. */
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
		{ "q=a",              "q",            opt_success,  "a"      }, /* Short keyword string. */
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
		opt_t retv = find_opt_value(test_data[i].input, test_data[i].searched_key, &value);
		if (retv != test_data[i].expected_retv) {
			fprintf(stderr, "[EE] find_opt_value(%s, %s, ...) returns unexpected retv: retv = %u, expected = %u in test #%zu\n",
			        test_data[i].input, test_data[i].searched_key,
			        retv, test_data[i].expected_retv, i);
			return -1;
		}
		if (retv == opt_success) {
			if (0 != strcmp(value, test_data[i].expected_value)) {
				fprintf(stderr, "[EE] find_opt_value(%s, %s, ...) returns unexpected value: value = [%s], expected = [%s] in test #%zu\n",
				        test_data[i].input, test_data[i].searched_key,
				        value, test_data[i].expected_value, i);
				return -1;
			}
		}
	}

	return 0;
}




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
		{ "74Morse",              false, doesnt_matter }, /* Some characters aren't digits. */
		{ "74ac45",               false, doesnt_matter }, /* Some characters aren't decimal digits. */
		{ "four",                 false, doesnt_matter }, /* None of characters are decimal digits. */
		{ "\03345",               false, doesnt_matter }, /* Leading non-digit, non-space character. 033(oct) = 27(dec) = 0x1b = ESC. */


		/* Success cases. */
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
			fprintf(stderr, "[EE] Unexpected return value in test case %zu / %zu: %d\n",
			        i + 1, n_test_cases, retv);
			return -1;
		}

		if (retv == false) {
			/* cwdaemon_get_long() failed to convert input string. No need to
			   make further checks. */
			continue;
		}

		if (value != tcase->expected_value) {
			fprintf(stderr, "[EE] Unexpected converted value in test case %zu / %zu: %ld (expected %ld)\n",
			        i + 1, n_test_cases, value, tcase->expected_value);
			return -1;
		}
	}

	fprintf(stderr, "[II] Tests of cwdaemon_get_long() have succeeded\n");

	return 0;
}

