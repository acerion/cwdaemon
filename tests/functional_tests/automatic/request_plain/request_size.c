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
   Test cases that send to cwdaemon plain requests that have size (count of
   bytes) close to cwdaemon's maximum size of requests. The requests are
   slightly smaller, equal to and slightly larger than the size of cwdaemon's
   receive buffer.

   cwdaemon's buffer that is used to receive requests has
   CWDAEMON_REQUEST_SIZE_MAX==256 bytes. If a plain request sent to cwdaemon
   is larger than that, it will be truncated.
*/




#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcw.h>

#include "tests/library/log.h"

#include "request_size.h"
#include "shared.h"




/* Helper definition to shorten strings in test cases. Bytes at position 21
   till 250, inclusive. */
#define BYTES_11_250 \
	          "kukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku"




static test_case_t g_test_cases[] = {
	/*
	  In these cases a full plain request is keyed on cwdevice and received
	  by Morse code receiver.
	*/
	{ .description = "plain request with size smaller than cwdaemon's receive buffer - 254 bytes (without NUL)",
	  .plain_request          = TESTS_SET_BYTES("paris 7890" BYTES_11_250 "1234"),
	  .expected_morse_receive =                 "paris 7890" BYTES_11_250 "1234",
	  .expected_events        = { { .etype = etype_morse, }, },
	},
	{ .description = "plain request with size smaller than cwdaemon's receive buffer - 254+1 bytes (with NUL)",
	  .plain_request          = TESTS_SET_BYTES("paris 7890" BYTES_11_250 "1234\0"),
	  .expected_morse_receive =                 "paris 7890" BYTES_11_250 "1234",
	  .expected_events        = { { .etype = etype_morse  }, },
	},
	{ .description = "plain request with size smaller than cwdaemon's receive buffer - 255 bytes (without NUL)",
	  .plain_request          = TESTS_SET_BYTES("paris 7890" BYTES_11_250 "12345"),
	  .expected_morse_receive =                 "paris 7890" BYTES_11_250 "12345",
	  .expected_events        = { { .etype = etype_morse  }, },
	},



	/*
	  In these cases a full plain request is keyed on cwdevice and received
	  by Morse code receiver.
	*/
	{ .description = "plain request with size equal to cwdaemon's receive buffer - 255+1 bytes (with NUL)",
	  .plain_request          = TESTS_SET_BYTES("paris 7890" BYTES_11_250 "12345\0"),
	  .expected_morse_receive =                 "paris 7890" BYTES_11_250 "12345",
	  .expected_events        = { { .etype = etype_morse  }, },
	},
	{ .description = "plain request with size equal to cwdaemon's receive buffer - 256 bytes (without NUL)",
	  .plain_request          = TESTS_SET_BYTES("paris 7890" BYTES_11_250 "123456"),
	  .expected_morse_receive =                 "paris 7890" BYTES_11_250 "123456",
	  .expected_events        = { { .etype = etype_morse  }, },
	},



	/*
	  In this case a full plain request is keyed on cwdevice and received by
	  Morse code receiver.

	  In this case cwdaemon's receive code will drop only the terminating
	  NUL. The non-present NUL will have no impact on further actions of
	  cwdaemon or on contents of keyed Morse message.
	*/
	{ .description = "plain request with size larger than cwdaemon's receive buffer - 256+1 bytes (with NUL)",
	  .plain_request          = TESTS_SET_BYTES("paris 7890" BYTES_11_250 "123456\0"),
	  .expected_morse_receive =                 "paris 7890" BYTES_11_250 "123456",
	  .expected_events        = { { .etype = etype_morse  }, },
	},



	/*
	  In these cases only a truncated plain request is keyed on cwdevice and
	  received by Morse code receiver. Request's bytes that won't fit into
	  cwdaemon's receive buffer will be dropped by cwdaemon's receive code
	  and won't be keyed on cwdevice.

	  These cases could be described as "failure cases" because Morse
	  receiver will return something else than what client has sent to
	  cwdaemon server. But we know that cwdaemon server will drop extra
	  byte(s) from the plain request, and we know what cwdaemon server will
	  key on cwdevice. And these test cases are expecting and testing exactly
	  this behaviour.

	  Morse receiver will receive only first 256 bytes. This is what Morse
	  code receiver will receive when this test tries to play a message with
	  count of bytes that is larger than cwdaemon's receive buffer (the
	  receive buffer has space for 256 bytes). The last byte(s) from request
	  will be dropped by cwdaemon's receive code.
	*/
	{ .description = "plain request with size larger than cwdaemon's receive buffer - 257 bytes (without NUL); TRUNCATION of Morse receive",
	  .plain_request          = TESTS_SET_BYTES("paris 7890" BYTES_11_250 "1234567"),
	  .expected_morse_receive =                 "paris 7890" BYTES_11_250 "123456",
	  .expected_events        = { { .etype = etype_morse  }, },
	},
	{ .description = "plain request with size larger than cwdaemon's receive buffer - 257+1 bytes (with NUL); TRUNCATION of Morse receive",
	  .plain_request          = TESTS_SET_BYTES("paris 7890" BYTES_11_250 "1234567\0"),
	  .expected_morse_receive =                 "paris 7890" BYTES_11_250 "123456",
	  .expected_events        = { { .etype = etype_morse  }, },
	},
};




int request_size_tests(const test_options_t * test_opts)
{
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	const int rv = run_test_cases(g_test_cases, n_test_cases, test_opts);

	if (0 != rv) {
		test_log_err("Test: result of the 'request size' test: FAIL %s\n", "");
		test_log_newline(); /* Visual separator. */
		return -1;
	}
	test_log_info("Test: result of the 'request size' test: PASS %s\n", "");
	test_log_newline(); /* Visual separator. */
	return 0;
}

