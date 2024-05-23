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

   Test cases that send to cwdaemon REPLY Escape requests that have size
   (count of characters) close to cwdaemon's maximum size of requests. The
   requests are slightly smaller, equal to and slightly larger than the size
   of cwdaemon's buffer.

   cwdaemon's buffer that is used to receive requests has
   CWDAEMON_REQUEST_SIZE_MAX==256 bytes.

   Really long requests that have better chance of triggering a crash of
   cwdaemon are sent to the server in fuzzing test in another directory.
*/




#include "tests/library/log.h"

#include "request_size.h"
#include "shared.h"




/* Bytes from 11 to 250 (inclusive) in Escape request. */
#define ESC_BYTES_250 \
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




/// @reviewed_on{2024.05.01}
static const test_case_t g_test_cases[] = {
	{ .description   = "REPLY Escape request with size smaller than cwdaemon's receive buffer - 254 bytes (without NUL)",
	  .esc_request   =                                 TESTS_SET_BYTES("\033hGREEN 90" ESC_BYTES_250 "1234"),
	  .plain_request =                                 TESTS_SET_BYTES("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "hGREEN 90" ESC_BYTES_250 "1234\r\n"),},
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("liverpool0" PLAIN_BYTES_250 "123456"),   }, },
	},

	{ .description   = "REPLY Escape request with size smaller than cwdaemon's receive buffer - 254+1 bytes (with NUL)",
	  .esc_request   =                                 TESTS_SET_BYTES("\033hsongs 90" ESC_BYTES_250 "1234\0"),
	  .plain_request =                                 TESTS_SET_BYTES("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "hsongs 90" ESC_BYTES_250 "1234\r\n"),},
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("liverpool0" PLAIN_BYTES_250 "123456"),   }, },
	},

	{ .description   = "REPLY Escape request with size smaller than cwdaemon's receive buffer - 255 bytes (without NUL)",
	  .esc_request   =                                 TESTS_SET_BYTES("\033hparis 90" ESC_BYTES_250 "12345"),
	  .plain_request =                                 TESTS_SET_BYTES("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "hparis 90" ESC_BYTES_250 "12345\r\n"),},
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("liverpool0" PLAIN_BYTES_250 "123456"),    }, },
	},

	{ .description   = "REPLY Escape request with size equal to cwdaemon's receive buffer - 255+1 bytes (with NUL)",
	  .esc_request   =                                 TESTS_SET_BYTES("\033hmarch 90" ESC_BYTES_250 "12345\0"),
	  .plain_request =                                 TESTS_SET_BYTES("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "hmarch 90" ESC_BYTES_250 "12345\r\n"),},
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("liverpool0" PLAIN_BYTES_250 "123456"),    }, },
	},

	{ .description   = "REPLY Escape request with size equal to cwdaemon's receive buffer - 256 bytes (without NUL)",
	  .esc_request   =                                 TESTS_SET_BYTES("\033hearth 90" ESC_BYTES_250 "123456"),
	  .plain_request =                                 TESTS_SET_BYTES("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "hearth 90" ESC_BYTES_250 "123456\r\n"),},
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("liverpool0" PLAIN_BYTES_250 "123456"),     }, },
	},

	{ .description   = "REPLY Escape request with size larger than cwdaemon's receive buffer - 256+1 bytes (with NUL)",
	  /* The '\0' char from Escape request will be dropped in daemon during
	     receive - it won't fit into receive buffer. */
	  .esc_request   =                                 TESTS_SET_BYTES("\033hwindy 90" ESC_BYTES_250 "123456\0"),
	  .plain_request =                                 TESTS_SET_BYTES("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "hwindy 90" ESC_BYTES_250 "123456\r\n"),},
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("liverpool0" PLAIN_BYTES_250 "123456"),     }, },
	},




	/// In the following test cases a truncation of request will occur. First
	/// cwdaemon will drop the last non-NULL char(s), and then the daemon
	/// will send back truncated reply.
	{ .description   = "REPLY Escape request with size larger than cwdaemon's receive buffer - 257 bytes (without NUL) - TRUNCATION of reply",
	  /* The '7' char from Escape request will be dropped in daemon during
	     receive - it won't fit into receive buffer. */
	  .esc_request   =                                 TESTS_SET_BYTES("\033htable 90" ESC_BYTES_250 "1234567"),
	  .plain_request =                                 TESTS_SET_BYTES("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "htable 90" ESC_BYTES_250 "123456\r\n"),},
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("liverpool0" PLAIN_BYTES_250 "123456"),     }, },
	},
	{ .description   = "REPLY Escape request with size larger than cwdaemon's receive buffer - 257+1 bytes (with NUL) - TRUNCATION of reply",
	  /* The '7' and '\0' chars from Escape request will be dropped in daemon
	     during receive - they won't fit into receive buffer. */
	  .esc_request   =                                 TESTS_SET_BYTES("\033hhorse 90" ESC_BYTES_250 "1234567\0"),
	  .plain_request =                                 TESTS_SET_BYTES("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "hhorse 90" ESC_BYTES_250 "123456\r\n"),},
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("liverpool0" PLAIN_BYTES_250 "123456"),     }, },
	},
	{ .description   = "REPLY Escape request with size larger than cwdaemon's receive buffer - 258 bytes (without NUL) - TRUNCATION of reply",
	  /* The '7' and '8' chars from Escape request will be dropped in daemon
	     during receive - they won't fit into receive buffer. */
	  .esc_request   =                                 TESTS_SET_BYTES("\033hhotel 90" ESC_BYTES_250 "12345678"),
	  .plain_request =                                 TESTS_SET_BYTES("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "hhotel 90" ESC_BYTES_250 "123456\r\n"),},
	                { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("liverpool0" PLAIN_BYTES_250 "123456"),     }, },
	},
	{ .description   = "REPLY Escape request with size larger than cwdaemon's receive buffer - 258+1 bytes (with NUL) - TRUNCATION of reply",
	  /* The '7', '8' and '\0' chars from Escape request will be dropped in
	     daemon during receive - they won't fit into receive buffer. */
	  .esc_request   =                                  TESTS_SET_BYTES("\033hspain 90" ESC_BYTES_250 "12345678\0"),
	  .plain_request =                                  TESTS_SET_BYTES("liverpool0" PLAIN_BYTES_250 "123456"),
	  .expected  = { { .etype = etype_reply, .u.reply = TESTS_SET_BYTES(    "hspain 90" ESC_BYTES_250 "123456\r\n"),},
	                 { .etype = etype_morse, .u.morse = TESTS_SET_MORSE("liverpool0" PLAIN_BYTES_250 "123456"),     }, },
	},
};




int request_size_tests(test_options_t const * test_opts)
{
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	return run_test_cases(g_test_cases, n_test_cases, test_opts, "REPLY Escape request - request size");
}

