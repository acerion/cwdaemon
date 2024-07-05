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
/// Basic tests of PORT Escape request. Request a change of port and observe
/// if/how a Morse message is keyed on cwdevice after processing the PORT
/// Escape request.
///
/// The PORT Escape request is obsoleted and has no impact on working of
/// cwdaemon. But until it is completely removed from cwdaemon, I want to
/// test that it has no impact on working on cwdaemon. I don't want to do the
/// test completely manually, hence the supervised test.




#include <libcw.h>

#include "tests/library/events.h"
#include "tests/library/log.h"

#include "basic.h"
#include "shared.h"




/// There is only one test case because the differentiating factor - the port
/// - is picked at random in code executing the test case in a loop.
///
/// The test case is executed in a loop. Each iteration generates the same
/// sequence of events, but each iteration requests for different port.
///
/// @reviewed_on{2024.07.05}
static const test_case_t g_test_cases[] = {
	{ .description = "Using random port",
	  .reply_esc_request =                             TESTS_SET_BYTES("\033h"),
	  .plain_request =                                 TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "h\r\n") },
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris")     }, },

	},
};




// @reviewed_on{2024.07.05}
int basic_tests(test_options_t const * test_opts)
{
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	return run_test_cases(g_test_cases, n_test_cases, test_opts, "PORT Escape request - basic");
}

