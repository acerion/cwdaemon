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
/// Basic tests of CWDEVICE Escape request. Request a change of cwdevice and
/// observe if/how a Morse message is keyed on the new cwdevice.




#include "tests/library/events.h"
#include "tests/library/log.h"

#include "basic.h"
#include "shared.h"




/// @reviewed_on{2024.05.24}
static const test_case_t g_test_cases[] = {
	// In this test case there is no text received on cwdevice because we
	// explicitly ask for "null" device. And "null" cwdevice's purpose is to
	// provide no real action on neither keying nor ptt pins.
	{ .description = "Requesting 'null' cwdevice",
	  .get_cwdevice_name = get_null_cwdevice_name,
	  .reply_esc_request =                             TESTS_SET_BYTES("\033h"),
	  .plain_request =                                 TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "h\r\n") }, },

	},

	// In this test case there is no text received on cwdevice because upon
	// requesting an invalid cwdevice the cwdaemon server falls back to
	// "null" device. And "null" cwdevice's purpose is to provide no real
	// action on neither keying nor ptt pins.
	{ .description = "Falling back to 'null' cwdevice",
	  .get_cwdevice_name = get_invalid_cwdevice_name,
	  .reply_esc_request =                             TESTS_SET_BYTES("\033h"),
	  .plain_request =                                 TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "h\r\n") }, },

	},
	// In this test case we are requesting cwdaemon to use a cwdevice that is
	// present on a machine and in theory can be used as cwdevice, but is not
	// being observed by cwdevice observer.
	{ .description = "Requesting valid non-default cwdevice",
	  .get_cwdevice_name = get_valid_non_default_cwdevice_name,
	  .reply_esc_request =                             TESTS_SET_BYTES("\033h\0"),
	  .plain_request =                                 TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "h\r\n") }, },
	},
	// In this test case we are asking cwdaemon to use a cwdevice that was
	// passed to the test as THE cwdevice to be used by this test. This is a
	// real cwdevice observed by the cwdevice observer, with keying and ptt
	// pins, so we expect some Morse receive on a keying pin.
	{ .description = "Requesting test-default cwdevice",
	  .get_cwdevice_name = get_test_default_cwdevice_name,
	  .reply_esc_request =                             TESTS_SET_BYTES("\033h\0"),
	  .plain_request =                                 TESTS_SET_BYTES("paris"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "h\r\n") },
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris")     }, },
	},
};




// @reviewed_on{2024.05.11}
int basic_tests(test_options_t const * test_opts)
{
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	return run_test_cases(g_test_cases, n_test_cases, test_opts, "CWDEVICE Escape request - basic");
}

