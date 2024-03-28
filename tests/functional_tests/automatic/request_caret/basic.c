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
   Basic tests of caret ('^') request.

   The tests are basic because a single test case just sends one caret
   requests and sees what happens.

   TODO acerion 2024.01.26: add "advanced" tests (in separate file) in which
   there is some client code that waits for server's response and interacts
   with it, perhaps by sending another caret request, and then another, and
   another. Come up with some good methods of testing of more advanced
   scenarios.
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

  TODO acerion 2024.02.18: make sure that the description of caret message
  contains the information that socket reply includes all characters from
  original message, including invalid characters that weren't keyed on
  cwdevice.

  TODO acerion 2024.02.18: make sure that similar test is added for
  regular/plain message requests in the future.
*/




static test_case_t g_test_cases[] = {
	{ .description = "mixed characters",
	  .caret_request              = TEST_SET_BYTES("22 crows, 1 stork?^"),
	  .expected_socket_reply      = TEST_SET_BYTES("22 crows, 1 stork?\r\n"),
	  .expected_morse_receive     =                "22 crows, 1 stork?",
	  .expected_events            = { { .event_type = event_type_socket_receive },
	                                  { .event_type = event_type_morse_receive  }, },
	},



	/*
	  Handling of caret in cwdaemon indicates that once a first caret is
	  recognized, the caret and everything after it is ignored:

	      case '^':
	          *x = '\0';     // Remove '^' and possible trailing garbage.
	*/
	{ .description = "additional message after caret",
	  .caret_request              = TEST_SET_BYTES("Fun^Joy^"),
	  .expected_socket_reply      = TEST_SET_BYTES("Fun\r\n"),
	  .expected_morse_receive     =                "Fun",
	  .expected_events            = { { .event_type = event_type_socket_receive },
	                                  { .event_type = event_type_morse_receive  }, },
	},
	{ .description = "message with two carets",
	  .caret_request              = TEST_SET_BYTES("Monday^^"),
	  .expected_socket_reply      = TEST_SET_BYTES("Monday\r\n"),
	  .expected_morse_receive     =                "Monday",
	  .expected_events            = { { .event_type = event_type_socket_receive },
	                                  { .event_type = event_type_morse_receive  }, },
	},



	{ .description = "two words",
	  .caret_request              = TEST_SET_BYTES("Hello world!^"),
	  .expected_socket_reply      = TEST_SET_BYTES("Hello world!\r\n"),
	  .expected_morse_receive     =                "Hello world!",
	  .expected_events            = { { .event_type = event_type_socket_receive },
	                                  { .event_type = event_type_morse_receive  }, },
	},

	/* There should be no action from cwdaemon: neither keying nor socket
	   reply. */
	{ .description = "empty text - no terminating NUL in request",
	  .caret_request              = TEST_SET_BYTES("^"),
	  .expected_socket_reply      = TEST_SET_BYTES(""),
	  .expected_morse_receive     =                "",
	  .expected_events            = { { 0 } },
	},

	/* There should be no action from cwdaemon: neither keying nor socket
	   reply. */
	{ .description = "empty text - with terminating NUL in request",
	  .caret_request              = TEST_SET_BYTES("^\0"), /* Explicit terminating NUL. The NUL will be ignored by cwdaemon. */
	  .expected_socket_reply      = TEST_SET_BYTES(""),
	  .expected_morse_receive     =                "",
	  .expected_events            = { { 0 } },
	},

	{ .description = "single character",
	  .caret_request              = TEST_SET_BYTES("f^"),
	  .expected_socket_reply      = TEST_SET_BYTES("f\r\n"),
	  .expected_morse_receive     =                "f",
	  .expected_events            = { { .event_type = event_type_socket_receive },
	                                  { .event_type = event_type_morse_receive  }, },
	},

	{ .description = "single word - no terminating NUL in request",
	  .caret_request              = TEST_SET_BYTES("Paris^"),
	  .expected_socket_reply      = TEST_SET_BYTES("Paris\r\n"),
	  .expected_morse_receive     =                "Paris",
	  .expected_events            = { { .event_type = event_type_socket_receive },
	                                  { .event_type = event_type_morse_receive  }, },
	},

	{ .description = "single word - with terminating NUL in request",
	  .caret_request              = TEST_SET_BYTES("Paris^\0"), /* Explicit terminating NUL. The NUL will be ignored by cwdaemon. */
	  .expected_socket_reply      = TEST_SET_BYTES("Paris\r\n"),
	  .expected_morse_receive     =                "Paris",
	  .expected_events            = { { .event_type = event_type_socket_receive },
	                                  { .event_type = event_type_morse_receive  }, },
	},

	/* Notice how the leading space from message is preserved in socket reply. */
	{ .description = "single word with leading space",
	  .caret_request              = TEST_SET_BYTES(" London^"),
	  .expected_socket_reply      = TEST_SET_BYTES(" London\r\n"),
	  .expected_morse_receive     =                 "London",
	  .expected_events            = { { .event_type = event_type_socket_receive },
	                                  { .event_type = event_type_morse_receive  }, },
	},

	/* Notice how the trailing space from message is preserved in socket reply. */
	{ .description = "mixed characters with trailing space",
	  .caret_request              = TEST_SET_BYTES("when, now = right: ^"),
	  .expected_socket_reply      = TEST_SET_BYTES("when, now = right: \r\n"),
	  .expected_morse_receive     =                "when, now = right:",
	  .expected_events            = { { .event_type = event_type_socket_receive },
	                                  { .event_type = event_type_morse_receive  }, },
	},

	/* Refer to comment starting with "Info for test case with '-1' byte."
	   above for more info. */
	{ .description                = "message containing '-1' integer value",
	  .caret_request              = { .n_bytes = 10, .bytes = { 'p', 'a', 's', 's', 'e', 'n', -1, 'e', 'r', '^', } },
	  .expected_socket_reply      = { .n_bytes = 11, .bytes = { 'p', 'a', 's', 's', 'e', 'n', -1, 'e', 'r', '\r', '\n' } },      /* cwdaemon sends verbatim text in socket reply. */
	  .expected_morse_receive     =                           { 'p', 'a', 's', 's', 'e', 'n',     'e', 'r', '\0' },              /* Morse message keyed on cwdevice must not contain the -1 char (the char should be skipped by cwdaemon). */
	  .expected_events            = { { .event_type = event_type_socket_receive },
	                                  { .event_type = event_type_morse_receive  }, },
	},
};





int basic_caret_test(const test_options_t * test_opts)
{

	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	int rv = run_test_cases(g_test_cases, n_test_cases, test_opts);

	if (0 != rv) {
		test_log_err("Test: result of the 'basic' test: FAIL %s\n", "");
		test_log_newline(); /* Visual separator. */
		return -1;
	}
	test_log_info("Test: result of the 'basic' test: PASS %s\n", "");
	test_log_newline(); /* Visual separator. */
	return 0;
}

