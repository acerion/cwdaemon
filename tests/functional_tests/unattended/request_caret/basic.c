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
   @file

   Basic tests of caret ('^') request.

   The tests are basic because a single test case just sends one caret
   requests and sees what happens.

   TODO acerion 2024.01.26: add "advanced" tests (in separate file) in which
   there is some client code that waits for server's response and interacts
   with it, perhaps by sending another caret request, and then another, and
   another. In other words, come up with some good methods of testing of more
   advanced scenarios.
*/




#include <stdlib.h>

#include "tests/library/events.h"
#include "tests/library/log.h"

#include "basic.h"
#include "shared.h"




/*
  Info for test case with '-1' byte.

  Data for testing how cwdaemon handles a bug in libcw.

  libcw 8.0.0 from unixcw 3.6.1 crashes when enqueued character has value
  ((char) -1) / ((unsigned char) 255). This has been fixed in unixcw commit
  c4fff9622c4e86c798703d637be7cf7e9ab84a06.

  Since cwdaemon has to still work with unfixed versions of library, it has
  to skip (not enqueue) the character.

  The problem is worked-around in cwdaemon by adding 'is_valid' condition
  before calling cw_send_character().

  TODO acerion 2024.02.18: this functional test doesn't display information
  that cwdaemon which doesn't have a workaround is experiencing a crash. It
  would be good to know in all functional tests that cwdaemon has crashed -
  it would give more info to tester.

  TODO acerion 2024.02.18: make sure that the description of caret message in
  cwdaemon's documentation contains the information that reply
  includes all characters from original message, including invalid characters
  that weren't keyed on cwdevice.

  TODO acerion 2024.02.18: make sure that similar test is added for
  regular/plain message requests in the future.
*/




/// @brief Test cases for basic tests
///
/// @reviewed_on{2024.04.30}
static test_case_t g_test_cases[] = {
	{ .description   = "mixed characters",
	  .caret_request =                                 TESTS_SET_BYTES("22 crows, 1 stork?^"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES("22 crows, 1 stork?\r\n"), },
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("22 crows, 1 stork?"),     }, },
	},


	// Handling of caret in cwdaemon indicates that once a first caret is
	// recognized, the caret and everything after it is ignored:
	//
	//    case '^':
	//        *x = '\0';     // Remove '^' and possible trailing garbage.
	{ .description   = "additional message after caret",
	  .caret_request =                                 TESTS_SET_BYTES("Fun^Joy^"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES("Fun\r\n") },
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("Fun"),    }, },
	},
	{ .description   = "message with two carets",
	  .caret_request =                                 TESTS_SET_BYTES("Monday^^"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES("Monday\r\n") },
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("Monday"),    }, },
	},


	{ .description   = "two words",
	  .caret_request =                                 TESTS_SET_BYTES("Hello world!^"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES("Hello world!\r\n") },
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("Hello world!"),    }, },
	},


	// There should be no action from cwdaemon: neither keying nor reply.
	{ .description   = "empty text - no terminating NUL in request",
	  .caret_request = TESTS_SET_BYTES("^"),
	  .expected = { { 0 } },
	},


	// There should be no action from cwdaemon: neither keying nor reply.
	// Explicit terminating NUL will be ignored by cwdaemon.
	{ .description   = "empty text - with terminating NUL in request",
	  .caret_request = TESTS_SET_BYTES("^\0"), /*  */
	  .expected = { { 0 } },
	},


	{ .description   = "single character",
	  .caret_request =                                 TESTS_SET_BYTES("f^"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES("f\r\n") },
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("f"),    }, },
	},


	{ .description   = "single word - no terminating NUL in request",
	  .caret_request =                                 TESTS_SET_BYTES("Paris^"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES("Paris\r\n") },
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("Paris"),    }, },
	},


	// Request with explicit terminating NUL. The NUL will be ignored by
	// cwdaemon.
	{ .description   = "single word - with terminating NUL in request",
	  .caret_request =                                 TESTS_SET_BYTES("Paris^\0"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES("Paris\r\n") },
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("Paris"),    }, },
	},


	// Notice how the leading space from message is preserved in reply.
	{ .description   = "single word with leading space",
	  .caret_request =                                 TESTS_SET_BYTES(" London^"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(" London\r\n") },
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE( "London"),    }, },
	},


	// Notice how the trailing space from message is preserved in reply.
	//
	// TODO (acerion) 2024.04.29: explain why in this particular test case
	// the Morse event is expected before reply event.
	{ .description   = "mixed characters with trailing space",
	  .caret_request =                                 TESTS_SET_BYTES("when, now = right: ^"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("when, now = right:"),     },
	                { .etype = etype_reply, .u.reply = TESTS_SET_BYTES("when, now = right: \r\n") }, },
	},


	// Refer to comment starting with "Info for test case with '-1' byte."
	// above for more info about this test case.
	{ .description   = "message containing '-1' integer value",
	  .caret_request =                                 { .n_bytes = 10, .bytes = { 'p', 'a', 's', 's', 'e', 'n', -1, 'e', 'r', '^',       }, },
	  .expected = { { .etype = etype_reply, .u.reply = { .n_bytes = 11, .bytes = { 'p', 'a', 's', 's', 'e', 'n', -1, 'e', 'r', '\r', '\n' }, }, },    // cwdaemon sends verbatim text in reply.
	                { .etype = etype_morse, .u.morse = { .string               = { 'p', 'a', 's', 's', 'e', 'n',     'e', 'r', '\0'       }, }, }, }, // Morse message keyed on cwdevice must not contain the -1 char (the char should be skipped by cwdaemon).
	},
};




/// @reviewed_on{2024.05.01}
int basic_caret_test(const test_options_t * test_opts)
{
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	return run_test_cases(g_test_cases, n_test_cases, test_opts, "caret request - basic");
}

