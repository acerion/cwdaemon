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

   Test cases that send to cwdaemon plain requests that have size (count of
   bytes) close to cwdaemon's maximum size of requests. The requests are
   slightly smaller, equal to and slightly larger than the size of cwdaemon's
   receive buffer.

   cwdaemon's buffer that is used to receive requests has
   CWDAEMON_REQUEST_SIZE_MAX==256 bytes. If a plain request sent to cwdaemon
   is larger than that, it will be truncated.

   Really long requests that have better chance of triggering a crash of
   cwdaemon are sent to the server in fuzzing test in another directory.
*/




#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcw.h>

#include "tests/library/log.h"

#include "request_size.h"
#include "shared.h"




/* Helper definition to shorten strings in test cases. Bytes at position 11
   till 250, inclusive. */
#define BYTES_11_250 \
	          "kukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku"




/// @reviewed_on{2024.05.01}
static test_case_t g_test_cases[] = {
	/*
	  In these cases a full plain request is keyed on cwdevice and received
	  by Morse code receiver.
	*/
	{ .description   = "PLAIN request with size smaller than cwdaemon's receive buffer - 254 bytes (without NUL)",
	  .plain_request =                                 TESTS_SET_BYTES("paris 7890" BYTES_11_250 "1234"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris 7890" BYTES_11_250 "1234"), }, },
	},
	{ .description   = "PLAIN request with size smaller than cwdaemon's receive buffer - 254+1 bytes (with NUL)",
	  .plain_request =                                 TESTS_SET_BYTES("paris 7890" BYTES_11_250 "1234\0"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris 7890" BYTES_11_250 "1234"),  }, },
	},
	{ .description   = "PLAIN request with size smaller than cwdaemon's receive buffer - 255 bytes (without NUL)",
	  .plain_request =                                 TESTS_SET_BYTES("paris 7890" BYTES_11_250 "12345"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris 7890" BYTES_11_250 "12345"),  }, },
	},



	/*
	  In these cases a full PLAIN request is keyed on cwdevice and received
	  by Morse code receiver.
	*/
	{ .description   = "PLAIN request with size equal to cwdaemon's receive buffer - 255+1 bytes (with NUL)",
	  .plain_request =                                 TESTS_SET_BYTES("paris 7890" BYTES_11_250 "12345\0"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris 7890" BYTES_11_250 "12345"),  }, },
	},
	{ .description   = "PLAIN request with size equal to cwdaemon's receive buffer - 256 bytes (without NUL)",
	  .plain_request =                                 TESTS_SET_BYTES("paris 7890" BYTES_11_250 "123456"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris 7890" BYTES_11_250 "123456"),  }, },
	},



	/*
	  In this case a full PLAIN request is keyed on cwdevice and received by
	  Morse code receiver.

	  In this case cwdaemon's receive code will drop only the terminating
	  NUL. The non-present NUL will have no impact on further actions of
	  cwdaemon or on contents of keyed Morse message.
	*/
	{ .description   = "PLAIN request with size larger than cwdaemon's receive buffer - 256+1 bytes (with NUL)",
	  .plain_request =                                 TESTS_SET_BYTES("paris 7890" BYTES_11_250 "123456\0"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris 7890" BYTES_11_250 "123456"),  }, },
	},



	/*
	  In these cases only a truncated PLAIN request is keyed on cwdevice and
	  received by Morse code receiver. Request's bytes that won't fit into
	  cwdaemon's receive buffer will be dropped by cwdaemon's receive code
	  and won't be keyed on cwdevice.

	  These cases could be described as "failure cases" because Morse
	  receiver will return something else than what client has sent to
	  cwdaemon server. But we know that cwdaemon server will drop extra
	  byte(s) from the PLAIN request, and we know what cwdaemon server will
	  key on cwdevice. And these test cases are expecting and testing exactly
	  this behaviour.

	  Morse receiver will receive only first 256 bytes. This is what Morse
	  code receiver will receive when this test tries to play a message with
	  count of bytes that is larger than cwdaemon's receive buffer (the
	  receive buffer has space for 256 bytes). The last byte(s) from request
	  will be dropped by cwdaemon's receive code.
	*/
	{ .description   = "PLAIN request with size larger than cwdaemon's receive buffer - 257 bytes (without NUL); TRUNCATION of Morse receive",
	  .plain_request =                                 TESTS_SET_BYTES("paris 7890" BYTES_11_250 "1234567"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris 7890" BYTES_11_250 "123456"),  }, },
	},
	{ .description   = "PLAIN request with size larger than cwdaemon's receive buffer - 257+1 bytes (with NUL); TRUNCATION of Morse receive",
	  .plain_request =                                 TESTS_SET_BYTES("paris 7890" BYTES_11_250 "1234567\0"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris 7890" BYTES_11_250 "123456"),  }, },
	},
};




int request_size_tests(const test_options_t * test_opts)
{
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	return run_test_cases(g_test_cases, n_test_cases, test_opts, "PLAIN request - request size");
}

