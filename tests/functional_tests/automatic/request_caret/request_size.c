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

   Test cases that send to cwdaemon caret requests that have size (count of
   bytes) close to cwdaemon's maximum size of requests. The requests are
   slightly smaller, equal to and slightly larger than the size of cwdaemon's
   buffer.

   cwdaemon's buffer that is used to receive requests has
   CWDAEMON_REQUEST_SIZE_MAX==256 bytes. If a caret request sent to cwdaemon
   is larger than that, it will be truncated in receive code and the caret
   character will be dropped.
*/




#include <stdlib.h>

#include "tests/library/events.h"
#include "tests/library/log.h"

#include "request_size.h"
#include "shared.h"




/* Helper definition to shorten strings in test cases. Bytes no. 11-255,
   inclusive. */
#define BYTES_11_250 \
	          "kukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku"




/// @reviewed_on{2024.05.01}
static test_case_t g_test_cases[] = {
	{ .description   = "caret request with size smaller than cwdaemon's receive buffer - 255 bytes (without NUL)",
	  .caret_request =                                 TESTS_SET_BYTES("paris 7890" BYTES_11_250 "1234^"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES("paris 7890" BYTES_11_250 "1234\r\n") },
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris 7890" BYTES_11_250 "1234"),    }, },
	},

	{ .description   = "caret request with size equal to cwdaemon's receive buffer - 256 bytes (without NUL)",
	  .caret_request =                                 TESTS_SET_BYTES("paris 7890" BYTES_11_250 "12345^"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES("paris 7890" BYTES_11_250 "12345\r\n") },
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris 7890" BYTES_11_250 "12345"),    }, },
	},

	/* '^' is a byte no. 257, so it will be dropped by cwdaemon's receive
	   code. cwdaemon won't interpret this request as caret request, and
	   won't send any reply. */
	{ .description   = "caret request with size larger than cwdaemon's receive buffer - 257 bytes (without NUL)",
	  .caret_request =                                 TESTS_SET_BYTES("paris 7890" BYTES_11_250 "123456^"),
	  .expected = { { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("paris 7890" BYTES_11_250 "123456" ), }, },
	},
};




/// @reviewed_on{2024.05.01}
int request_size_caret_test(const test_options_t * test_opts)
{
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	return run_test_cases(g_test_cases, n_test_cases, test_opts, "caret request - request size");
}


