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
   Test cases that send to cwdaemon REPLY escape requests that have size
   (count of characters) close to cwdaemon's maximum size of requests. The
   requests are slightly smaller, equal to and slightly larger than the size
   of cwdaemon's buffer.

   cwdaemon's buffer that is used to receive requests has
   CWDAEMON_REQUEST_SIZE_MAX==256 bytes.
*/




#include "tests/library/log.h"

#include "request_size.h"
#include "shared.h"




/* Bytes from X to 240 in escape request. */
#define ESC_BYTES_240 \
              "kukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku"


/*
  Bytes from X to 250 in plain request.

  There may be no good reason to send all 256 bytes in play request, so this
  set of bytes may be defined as empty. Long plain requests are tested in
  other functional test.
*/
#if 0
#define PLAIN_BYTES_250                                  \
	          "kukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku"
#else
#define PLAIN_BYTES_250 ""
#endif




static test_case_t g_test_cases[] = {
	{ .description = "esc REPLY request with size smaller than cwdaemon's receive buffer - 254 bytes (without NUL)",

	  .esc_request            = SOCKET_BUF_SET("\033hparis 90" ESC_BYTES_240 "1234"),
	  .expected_socket_reply  = SOCKET_BUF_SET(    "hparis 90" ESC_BYTES_240 "1234\r\n"),

	  .plain_request          = SOCKET_BUF_SET("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected_morse_receive =                "liverpool0" PLAIN_BYTES_250 "123456",

	  .expected_events        = { { .event_type = event_type_socket_receive },
	                              { .event_type = event_type_morse_receive  }, },
	},

	{ .description = "esc REPLY request with size smaller than cwdaemon's receive buffer - 254+1 bytes (with NUL)",

	  .esc_request            = SOCKET_BUF_SET("\033hparis 90" ESC_BYTES_240 "1234\0"),
	  .expected_socket_reply  = SOCKET_BUF_SET(    "hparis 90" ESC_BYTES_240 "1234\r\n"),

	  .plain_request          = SOCKET_BUF_SET("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected_morse_receive =                "liverpool0" PLAIN_BYTES_250 "123456",

	  .expected_events        = { { .event_type = event_type_socket_receive },
	                              { .event_type = event_type_morse_receive  }, },
	},

	{ .description = "esc REPLY request with size smaller than cwdaemon's receive buffer - 255 bytes (without NUL)",

	  .esc_request            = SOCKET_BUF_SET("\033hparis 90" ESC_BYTES_240 "12345"),
	  .expected_socket_reply  = SOCKET_BUF_SET(    "hparis 90" ESC_BYTES_240 "12345\r\n"),

	  .plain_request          = SOCKET_BUF_SET("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected_morse_receive =                "liverpool0" PLAIN_BYTES_250 "123456",

	  .expected_events        = { { .event_type = event_type_socket_receive },
	                              { .event_type = event_type_morse_receive  }, },
	},

	{ .description = "esc REPLY request with size equal to cwdaemon's receive buffer - 255+1 bytes (with NUL)",

	  .esc_request            = SOCKET_BUF_SET("\033hparis 90" ESC_BYTES_240 "12345\0"),
	  .expected_socket_reply  = SOCKET_BUF_SET(    "hparis 90" ESC_BYTES_240 "12345\r\n"),

	  .plain_request          = SOCKET_BUF_SET("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected_morse_receive =                "liverpool0" PLAIN_BYTES_250 "123456",

	  .expected_events        = { { .event_type = event_type_socket_receive },
	                              { .event_type = event_type_morse_receive  }, },
	},

	{ .description = "esc REPLY request with size equal to cwdaemon's receive buffer - 256 bytes (without NUL)",

	  .esc_request            = SOCKET_BUF_SET("\033hparis 90" ESC_BYTES_240 "123456"),
	  .expected_socket_reply  = SOCKET_BUF_SET(    "hparis 90" ESC_BYTES_240 "123456\r\n"),

	  .plain_request          = SOCKET_BUF_SET("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected_morse_receive =                "liverpool0" PLAIN_BYTES_250 "123456",

	  .expected_events        = { { .event_type = event_type_socket_receive },
	                              { .event_type = event_type_morse_receive  }, },
	},

	{ .description = "esc REPLY request with size larger than cwdaemon's receive buffer - 256+1 bytes (with NUL)",

	  /* The '\0' char from esc request will be dropped in daemon during
	     receive - it won't fit into receive buffer. */
	  .esc_request            = SOCKET_BUF_SET("\033hparis 90" ESC_BYTES_240 "123456\0"),
	  .expected_socket_reply  = SOCKET_BUF_SET(    "hparis 90" ESC_BYTES_240 "123456\r\n"),

	  .plain_request          = SOCKET_BUF_SET("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected_morse_receive =                "liverpool0" PLAIN_BYTES_250 "123456",

	  .expected_events        = { { .event_type = event_type_socket_receive },
	                              { .event_type = event_type_morse_receive  }, },
	},




	/* In the following test cases a truncation of request will occur. First
	   cwdaemon will drop the last non-NULL char(s), and then the daemon will
	   send back truncated reply. */
	{ .description = "esc REPLY request with size larger than cwdaemon's receive buffer - 257 bytes (without NUL) - TRUNCATION of reply",

	  /* The '7' char from esc request will be dropped in daemon during
	     receive - it won't fit into receive buffer. */
	  .esc_request            = SOCKET_BUF_SET("\033hparis 90" ESC_BYTES_240 "1234567"),
	  .expected_socket_reply  = SOCKET_BUF_SET(    "hparis 90" ESC_BYTES_240 "123456\r\n"),

	  .plain_request          = SOCKET_BUF_SET("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected_morse_receive =                "liverpool0" PLAIN_BYTES_250 "123456",

	  .expected_events        = { { .event_type = event_type_socket_receive },
	                              { .event_type = event_type_morse_receive  }, },
	},
	{ .description = "esc REPLY request with size larger than cwdaemon's receive buffer - 257+1 bytes (with NUL) - TRUNCATION of reply",

	  /* The '7' and '\0' chars from esc request will be dropped in daemon
	     during receive - they won't fit into receive buffer. */
	  .esc_request            = SOCKET_BUF_SET("\033hparis 90" ESC_BYTES_240 "1234567\0"),
	  .expected_socket_reply  = SOCKET_BUF_SET(    "hparis 90" ESC_BYTES_240 "123456\r\n"),

	  .plain_request          = SOCKET_BUF_SET("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected_morse_receive =                "liverpool0" PLAIN_BYTES_250 "123456",

	  .expected_events        = { { .event_type = event_type_socket_receive },
	                              { .event_type = event_type_morse_receive  }, },
	},
	{ .description = "esc REPLY request with size larger than cwdaemon's receive buffer - 258 bytes (without NUL) - TRUNCATION of reply",

	  /* The '7' and '8' chars from esc request will be dropped in daemon
	     during receive - they won't fit into receive buffer. */
	  .esc_request            = SOCKET_BUF_SET("\033hparis 90" ESC_BYTES_240 "12345678"),
	  .expected_socket_reply  = SOCKET_BUF_SET(    "hparis 90" ESC_BYTES_240 "123456\r\n"),

	  .plain_request          = SOCKET_BUF_SET("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected_morse_receive =                "liverpool0" PLAIN_BYTES_250 "123456",

	  .expected_events        = { { .event_type = event_type_socket_receive },
	                              { .event_type = event_type_morse_receive  }, },
	},
	{ .description = "esc REPLY request with size larger than cwdaemon's receive buffer - 258+1 bytes (with NUL) - TRUNCATION of reply",

	  /* The '7', '8' and '\0' chars from esc request will be dropped in
	     daemon during receive - they won't fit into receive buffer. */
	  .esc_request            = SOCKET_BUF_SET("\033hparis 90" ESC_BYTES_240 "12345678\0"),
	  .expected_socket_reply  = SOCKET_BUF_SET(    "hparis 90" ESC_BYTES_240 "123456\r\n"),

	  .plain_request          = SOCKET_BUF_SET("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected_morse_receive =                "liverpool0" PLAIN_BYTES_250 "123456",

	  .expected_events        = { { .event_type = event_type_socket_receive },
	                              { .event_type = event_type_morse_receive  }, },
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

