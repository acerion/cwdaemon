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
 * GNU Library General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   @file Unit tests for tests/library/string_utils.c
*/




#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "tests/library/log.h"
#include "tests/library/string_utils.h"




static int test_escape_string(void);




static int (*tests[])(void) = {
	test_escape_string,
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




/* TODO acerion 2024.02.18: add test cases that would result in escaped
   string that is larger than output buffer passes as an arg to tested
   function. */
static int test_escape_string(void)
{
#define OUTPUT_SIZE 55
	/*
	  All these cases are valid cases. Tested function should succeed in
	  building *some* path. The path may represent a non-existing device, but
	  it will always be a valid string starting with "/dev/".
	*/
	const struct {
		const char input[32];
		const char expected_output[OUTPUT_SIZE];
	} test_data[] = {
		/* String that doesn't need escaping. */
		{ .input = "",            .expected_output = ""            },
		{ .input = "Hello_WORLD", .expected_output = "Hello_WORLD" },

		/* \r\n, found in socket reply. */
		{ .input = "\r",                       .expected_output = "{CR}"                                 },
		{ .input = "\n",                       .expected_output = "{LF}"                                 },
		{ .input = "\rHello_WORLD\n",          .expected_output = "{CR}Hello_WORLD{LF}"                  },
		{ .input = "\n\r\rHello_WORLD\n\n\r",  .expected_output = "{LF}{CR}{CR}Hello_WORLD{LF}{LF}{CR}"  },

		/* -1 integer in the middle. Testing -1 because the value was
            involved in 39fd657fd62942e4d13e198a3dc2d7d7eb6d3920, and I want
            to be able to nicely print strings with such character. */
		{ .input = { -1, '\0' },                                       .expected_output = "{0xff}"                                                  },
		{ .input = { 'a', ' ', 'b' , 'c', -1, 'd', 'e', '\0' },        .expected_output = "a bc{0xff}de"                                            },
		{ .input = { -1, 'a', ' ', 'b' , -1, -1, 'd', 'e', -1, '\0' }, .expected_output = "{0xff}a b{0xff}{0xff}de{0xff}"                           },
		{ .input = { -1, -1, -1, -1, -1, -1, -1, -1, -1, '\0' },       .expected_output = "{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}"  },

		/* Mix of \r, \n, -1 and other non-printable chars. Plus some printable. */
		{ .input = { '\r', '\n', '\b', -1, 127, '\a', 27, -1, 65, '\0' },  .expected_output = "{CR}{LF}{0x08}{0xff}{0x7f}{0x07}{0x1b}{0xff}A"  },

		/* Properly escaped input string would not fit into output buffer, so
		   escaping function may add '#' at end of output. */
		{ .input           = {     -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  '\0' },
		  .expected_output =   "{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}"
		},
		{ .input           = { 'a', -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, '\0' },
		  .expected_output =   "a{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}{0xff}#####"
		},
		{ .input           = {   10, 13, 13, 13, 10, 13, 13, 13, 10, 13, 13, 13, 10,13, '\0' },
		  .expected_output =   "{LF}{CR}{CR}{CR}{LF}{CR}{CR}{CR}{LF}{CR}{CR}{CR}{LF}##"
		},
	};

	const size_t n_tests = sizeof (test_data) / sizeof (test_data[0]);
	for (size_t i = 0; i < n_tests; i++) {
		char output[OUTPUT_SIZE] = { 0 };
		escape_string(test_data[i].input, output, sizeof (output));
		if (0 != memcmp(test_data[i].expected_output, output, OUTPUT_SIZE)) {
			test_log_err("Test: test case #%02zu / %02zu fails, escape_string() gives wrong return value [%s], expected [%s]\n",
			             i + 1, n_tests,
			             output,
			             test_data[i].expected_output);
			return -1;
		}
		test_log_debug("Test: test case #%02zu / %02zu passes, escaped string is [%s]\n",
		               i + 1, n_tests, output);
	}

	return 0;
}

