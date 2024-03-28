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




/**
   Basic test cases for "plain request" messages.
*/




#include "basic.h"
#include "shared.h"

#include "tests/library/log.h"
#include "tests/library/test_defines.h"




static test_case_t g_test_cases[] = {
	{ .description = "success case: short plain request",
	  .plain_request          = TEST_SET_BYTES("paris abc"),
	  .expected_morse_receive =                "paris abc",
	  .expected_events        = { { .event_type = event_type_morse_receive  }, },
	},
};




int basic_tests(const test_options_t * test_opts)
{
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	const int rv = run_test_cases(g_test_cases, n_test_cases, test_opts);

	if (0 != rv) {
		test_log_err("Test: result of the 'basic' test: FAIL %s\n", "");
		test_log_newline(); /* Visual separator. */
		return -1;
	}
	test_log_info("Test: result of the 'basic' test: PASS %s\n", "");
	test_log_newline(); /* Visual separator. */
	return 0;

}

