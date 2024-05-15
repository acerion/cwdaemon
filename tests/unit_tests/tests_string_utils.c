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
/// Unit tests for tests/library/string_utils.c.




//#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "tests/library/log.h"
#include "tests/library/string_utils.h"
#include "tests/library/test_defines.h"




static int test_get_printable_string(void);




static int (*tests[])(void) = {
	test_get_printable_string,
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




/// @reviewed_on{2024.04.24}
static int test_get_printable_string(void)
{
#define OUTPUT_SIZE 55
	const struct {
		struct {
			const char bytes[32];
			size_t n_bytes;
		} data;
		const char expected_output[OUTPUT_SIZE];
	} test_data[] = {
		/* String that doesn't contain non-printable characters. */
		{ .data = TESTS_SET_BYTES(""),            .expected_output = ""            },
		{ .data = TESTS_SET_BYTES("Hello_WORLD"), .expected_output = "Hello_WORLD" },

		/* \r\n, found in reply. */
		{ .data = TESTS_SET_BYTES("\r"),                       .expected_output = "{CR}"                                 },
		{ .data = TESTS_SET_BYTES("\n"),                       .expected_output = "{LF}"                                 },
		{ .data = TESTS_SET_BYTES("\rHello_WORLD\n"),          .expected_output = "{CR}Hello_WORLD{LF}"                  },
		{ .data = TESTS_SET_BYTES("\n\r\rHello_WORLD\n\n\r"),  .expected_output = "{LF}{CR}{CR}Hello_WORLD{LF}{LF}{CR}"  },

		// -1 integer in the middle. I'm using test data with -1 because a
		// -request containing '-1' triggered a bug in libcw, that I needed
		// -to work around in 39fd657fd62942e4d13e198a3dc2d7d7eb6d3920.
		//
		// Now that I know that this value triggered problems, I need to test
		// for it, and I need to be able to print '-1' nicely.
		{ .data = { .bytes = { -1       }, .n_bytes = 1 },    .expected_output = "{0xff}"       },
		{ .data = { .bytes = { -1, '\0' }, .n_bytes = 2 },    .expected_output = "{0xff}{NUL}"  },

		{ .data = { .bytes = { 'a', ' ', 'b', 'c', -1, 'd', 'e'       }, .n_bytes = 7, },    .expected_output = "a bc{0xff}de"       },
		{ .data = { .bytes = { 'a', ' ', 'b', 'c', -1, 'd', 'e', '\0' }, .n_bytes = 8, },    .expected_output = "a bc{0xff}de{NUL}"  },

		{ .data = { .bytes = { -1, 'a', ' ', 'b', -1, -1, 'd', 'e', -1       }, .n_bytes =  9, },    .expected_output = "{0xff}a b{0xff}{0xff}de{0xff}"       },
		{ .data = { .bytes = { -1, 'a', ' ', 'b', -1, -1, 'd', 'e', -1, '\0' }, .n_bytes = 10, },    .expected_output = "{0xff}a b{0xff}{0xff}de{0xff}{NUL}"  },

		/* NUL characters inside of array. */
		{ .data = { .bytes = { '\0', 'a', ' ', '\0', -1, -1, 'd', 'e', -1        }, .n_bytes =  9, },    .expected_output = "{NUL}a {NUL}{0xff}{0xff}de{0xff}"       },
		{ .data = { .bytes = { '\0', 'a', ' ', '\0', -1, -1, 'd', 'e', -1, '\0'  }, .n_bytes = 10, },    .expected_output = "{NUL}a {NUL}{0xff}{0xff}de{0xff}{NUL}"  },

		{ .data = { .bytes = { -1, -1, -1, -1, -1, -1, -1, -1       }, .n_bytes = 8, },    .expected_output = "{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}"       },
		{ .data = { .bytes = { -1, -1, -1, -1, -1, -1, -1, -1, '\0' }, .n_bytes = 9, },    .expected_output = "{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{NUL}"  },

		/* Mix of \r, \n, -1 and other non-printable chars. Plus some printable. */
		{ .data = { .bytes = { '\r', '\n', '\b', -1, 127, '\a', 27, -1, 65       }, .n_bytes =  9, },    .expected_output = "{CR}{LF}{0x08}{0xff}{0x7f}{0x07}{ESC}{0xff}A"       },
		{ .data = { .bytes = { '\r', '\n', '\b', -1, 127, '\a', 27, -1, 65, '\0' }, .n_bytes = 10, },    .expected_output = "{CR}{LF}{0x08}{0xff}{0x7f}{0x07}{ESC}{0xff}A{NUL}"  },

		/* Bytes string fully converted into printable form would not fit
		   into output buffer, so "get printable" function may add '#' at end
		   of output. */
		{ .data = { .bytes = {      -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1      }, .n_bytes =  9, },
		  .expected_output =    "{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}"
		},
		{ .data = { .bytes = { 'a', -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1      }, .n_bytes = 10, },
		  .expected_output =   "a{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}#####"
		},
		{ .data = { .bytes = {   10, 13, 13, 13, 10, 13, 13, 13, 10, 13, 13, 13,'\0',13     }, .n_bytes = 14, },
		  .expected_output =   "{LF}{CR}{CR}{CR}{LF}{CR}{CR}{CR}{LF}{CR}{CR}{CR}{NUL}#"
		},
	};

	const size_t n_tests = sizeof (test_data) / sizeof (test_data[0]);
	for (size_t i = 0; i < n_tests; i++) {
		char output[OUTPUT_SIZE] = { 0 };
		get_printable_string(test_data[i].data.bytes, test_data[i].data.n_bytes, output, sizeof (output));
		if (0 != memcmp(test_data[i].expected_output, output, OUTPUT_SIZE)) {
			test_log_err("Test: test case #%02zu / %02zu fails, get_printable_string() gives wrong return value [%s], expected [%s]\n",
			             i + 1, n_tests,
			             output,
			             test_data[i].expected_output);
			return -1;
		}
		test_log_debug("Test: test case #%02zu / %02zu passes, printable string is [%s]\n",
		               i + 1, n_tests, output);
	}

	return 0;
}

