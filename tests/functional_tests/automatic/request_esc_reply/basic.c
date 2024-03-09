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
   Basic tests of "'esc reply' request" feature: prepare a reply to be sent by cwdaemon.
*/




#include "tests/library/log.h"

#include "basic.h"
#include "shared.h"




static test_case_t g_test_cases[] = {
	/* This is a SUCCESS case. We request cwdaemon server to send us empty
	   string in reply. */
	{ .description             = "success case, empty reply value - no terminating NUL in esc request",

	  .esc_request             = SOCKET_BUF_SET("\033h"),
	  .expected_socket_reply   = SOCKET_BUF_SET(    "h\r\n"), /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = SOCKET_BUF_SET("paris"),
	  .expected_morse_receive  =                "paris",

	  .expected_events         = { { .event_type = event_type_socket_receive },
	                               { .event_type = event_type_morse_receive  }, },

	},

	/* This is a SUCCESS case. We request cwdaemon server to send us empty
	   string in reply. This time we add explicit NUL to end of esc
	   request. */
	{ .description             = "success case, empty reply value - with terminating NUL in esc request",

	  .esc_request             = SOCKET_BUF_SET("\033h\0"),   /* Notice the explicit terminating NUL. It will be ignored by daemon. */
	  .expected_socket_reply   = SOCKET_BUF_SET(    "h\r\n"), /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = SOCKET_BUF_SET("paris"),
	  .expected_morse_receive  =                "paris",

	  .expected_events         = { { .event_type = event_type_socket_receive },
	                               { .event_type = event_type_morse_receive  }, },
	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   single-letter string in reply. */
	{ .description             = "success case, single-letter as a value of reply",

	  .esc_request             = SOCKET_BUF_SET("\033hX"),
	  .expected_socket_reply   = SOCKET_BUF_SET(    "hX\r\n"), /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = SOCKET_BUF_SET("paris"),
	  .expected_morse_receive  =                "paris",

	  .expected_events         = { { .event_type = event_type_socket_receive },
	                               { .event_type = event_type_morse_receive  }, },
	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   single-word string in reply. */
	{ .description             = "success case, a word as value of reply, no terminating NUL in esc request",

	  .esc_request             = SOCKET_BUF_SET("\033hreply"),
	  .expected_socket_reply   = SOCKET_BUF_SET(    "hreply\r\n"), /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = SOCKET_BUF_SET("paris"),
	  .expected_morse_receive  =                "paris",

	  .expected_events         = { { .event_type = event_type_socket_receive },
	                               { .event_type = event_type_morse_receive  }, },
	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   single-word string in reply. This time we add explicit NUL to end of
	   esc request. */
	{ .description             = "success case, a word as value of reply, with terminating NUL in esc request",

	  .esc_request             = SOCKET_BUF_SET("\033hreply\0"),   /* Notice the explicit terminating NUL. It will be ignored by daemon. */
	  .expected_socket_reply   = SOCKET_BUF_SET(    "hreply\r\n"), /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = SOCKET_BUF_SET("paris"),
	  .expected_morse_receive  =                "paris",

	  .expected_events         = { { .event_type = event_type_socket_receive },
	                               { .event_type = event_type_morse_receive  }, },
	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   full-sentence string in reply. */
	{ .description             = "success case, a sentence as a value of reply",
	  .esc_request             = SOCKET_BUF_SET("\033hThis is a reply to your 27th request."),
	  .expected_socket_reply   = SOCKET_BUF_SET(    "hThis is a reply to your 27th request.\r\n"), /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = SOCKET_BUF_SET("paris"),
	  .expected_morse_receive  =                "paris",

	  .expected_events         = { { .event_type = event_type_socket_receive },
	                               { .event_type = event_type_morse_receive  }, },
	},

	/* This is a SUCCESS case which just skips keying a character with value (-1).

	   Test case for testing how cwdaemon handles a bug in libcw.

	   libcw 8.0.0 from unixcw 3.6.1 crashes when enqueued character has
	   value ((char) -1) / ((unsigned char) 255). This has been fixed in
	   unixcw commit c4fff9622c4e86c798703d637be7cf7e9ab84a06.

	   Since cwdaemon has to still work with unfixed versions of library, it
	   has to skip (not enqueue) the character.

	   The problem is worked-around in cwdaemon by adding 'is_valid'
	   condition before calling cw_send_character().

	   TODO acerion 2024.02.18: this functional test doesn't display
	   information that cwdaemon which doesn't have a workaround is
	   experiencing a crash. It would be good to know in all functional tests
	   that cwdaemon has crashed - it would give more info to tester.

	   TODO acerion 2024.02.18: make sure that the description of <ESC>h
	   request contains the information that socket reply includes all
	   characters from requested string, including "invalid" characters.

	   TODO acerion 2024.02.18: make sure that similar test is added for
	   regular/plain message requests in the future.
	*/
	{ .description             = "message containing '-1' integer value",

	  .esc_request             = { .n_bytes =  8, .bytes = { 27, 'h', 'l', -1,  'z', 'a', 'r', 'd' } },                /* cwdaemon doesn't validate values of chars that are requested for socket reply. */
	  .expected_socket_reply   = { .n_bytes =  9, .bytes =     { 'h', 'l', -1,  'z', 'a', 'r', 'd', '\r', '\n' } },  /* Notice the 'h' char prepended to a string from "requested reply". */

	  .plain_request           = { .n_bytes = 10, .bytes = { 'p', 'a', 's', 's', 'e', 'n', -1, 'e', 'r', '\0' } },  /* Notice inserted -1' */
	  .expected_morse_receive  =                           { 'p', 'a', 's', 's', 'e', 'n',     'e', 'r', '\0' },  /* Morse message keyed on cwdevice must not contain the -1 char (the char should be skipped by cwdaemon). */

	  .expected_events         = { { .event_type = event_type_socket_receive },
	                               { .event_type = event_type_morse_receive  }, },
	},
};




int basic_tests(void)
{
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	const int rv = run_test_cases(g_test_cases, n_test_cases);

	if (0 != rv) {
		test_log_err("Test: result of the 'basic' test: FAIL %s\n", "");
		test_log_newline(); /* Visual separator. */
		return -1;
	}
	test_log_info("Test: result of the 'basic' test: PASS %s\n", "");
	test_log_newline(); /* Visual separator. */
	return 0;
}

