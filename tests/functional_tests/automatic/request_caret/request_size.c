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
   Test cases that send to cwdaemon caret requests that have size (count of
   bytes) close to cwdaemon's maximum size of requests. The requests are
   slightly smaller, equal to and slightly larger than the size of cwdaemon's
   buffer.

   cwdaemon's buffer that is used to receive requests has
   CWDAEMON_REQUEST_SIZE_MAX==256 bytes. If a caret request sent to cwdaemon is
   larger than that, it will be truncated in receive code and the caret may
   be dropped.
*/




#include <stdlib.h>

#include "tests/library/events.h"
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
	{ .description = "caret request with size smaller than cwdaemon's receive buffer - 255 bytes (without NUL)",
	  .caret_request          = TEST_SET_BYTES("paris 7890" BYTES_11_250 "1234^"),
	  .expected_morse_receive =                "paris 7890" BYTES_11_250 "1234",
	  .expected_socket_reply  = TEST_SET_BYTES("paris 7890" BYTES_11_250 "1234\r\n"),
	  .expected_events        = { { .event_type = event_type_socket_receive },
	                              { .event_type = event_type_morse_receive  }, },
	},

	{ .description = "caret request with size equal to cwdaemon's receive buffer - 256 bytes (without NUL)",
	  .caret_request          = TEST_SET_BYTES("paris 7890" BYTES_11_250 "12345^"),
	  .expected_morse_receive =                "paris 7890" BYTES_11_250 "12345",
	  .expected_socket_reply  = TEST_SET_BYTES("paris 7890" BYTES_11_250 "12345\r\n"),
	  .expected_events        = { { .event_type = event_type_socket_receive },
	                              { .event_type = event_type_morse_receive  }, },
	},

	/* '^' is a byte no. 257, so it will be dropped by cwdaemon's receive
	   code. cwdaemon won't interpret this request as caret request, and
	   won't send anything over socket (reply is empty). */
	{ .description = "caret request with size larger than cwdaemon's receive buffer - 257 bytes (without NUL)",
	  .caret_request          = TEST_SET_BYTES("paris 7890" BYTES_11_250 "123456^"),
	  .expected_morse_receive =                "paris 7890" BYTES_11_250 "123456",
	  .expected_socket_reply  = TEST_SET_BYTES(""),
	  .expected_events        = { { .event_type = event_type_morse_receive  }, },
	},
};




int request_size_caret_test(const test_options_t * test_opts)
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


