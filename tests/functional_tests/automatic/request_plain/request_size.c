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
   Test cases that send to cwdaemon plain requests that have size (count of
   characters) close to cwdaemon's maximum size of requests. The requests are
   slightly smaller, equal to and slightly larger than the size of cwdaemon's
   buffer..

   cwdaemon's buffer used to receive network messages has
   CWDAEMON_MESSAGE_SIZE_MAX==256 bytes. If a plain request sent to cwdaemon
   is larger than that, it will be truncated.
*/




#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcw.h>

#include "request_size.h"
#include "shared.h"



/* Helper definition to shorten strings in test cases. Chars at position 21
   till 250, inclusive. */
#define CHARS_21_250 \
	                    "kukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku" \
	"kukukukukukukukukukukukukukukukukukukukukukukukuku"




static test_case_t g_test_cases[] = {
	/*
	  In this case a full plain request is keyed on cwdevice and received by
	  Morse code receiver.
	*/
	{ .description = "plain request with size smaller than cwdaemon's receive buffer - 254 bytes (without NUL)",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "1234"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "1234",
	},
	/*
	  In this case a full plain request is keyed on cwdevice and received by
	  Morse code receiver.
	*/
	{ .description = "plain request with size smaller than cwdaemon's receive buffer - 254+1 bytes (with NUL)",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "1234\0"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "1234",
	},



	/*
	  In this case a full plain request is keyed on cwdevice and received by
	  Morse code receiver.
	*/
	{ .description = "plain request with size smaller than cwdaemon's receive buffer - 255 bytes (without NUL)",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "12345"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "12345",
	},
	/*
	  In this case a full plain request is keyed on cwdevice and received by
	  Morse code receiver.
	*/
	{ .description = "plain request with size equal to cwdaemon's receive buffer - 255+1 bytes (with NUL)",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "12345\0"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "12345",
	},



	/*
	  In this case a full plain request is keyed on cwdevice and received by
	  Morse code receiver.
	*/
	{ .description = "plain request with size equal to cwdaemon's receive buffer - 256 bytes (without NUL)",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "123456"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "123456",
	},
	/*
	  In this case a full plain request is keyed on cwdevice and received by
	  Morse code receiver.

	  Notice that in this case cwdaemon's receive code will drop only the
	  terminating NUL. The non-present NUL will have no impact on further
	  actions of cwdaemon or on contents of keyed Morse message.
	*/
	{ .description = "plain request with size larger than cwdaemon's receive buffer - 256+1 bytes (with NUL)",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "123456\0"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "123456",
	},



	/*
	  In this case only a truncated plain request is keyed on cwdevice and
	  received by Morse code receiver. Request's characters that won't fit
	  into cwdaemon's receive buffer will be dropped by cwdaemon's receive
	  code and won't be keyed on cwdevice.

	  This case could be described as "failure case" because Morse receiver
	  will return something else than what client has sent to cwdaemon
	  server. But we know that cwdaemon server will drop extra char(s) from
	  the plain request, and we know what cwdaemon server will key on
	  cwdevice. And this test case is expecting and testing exactly this
	  behaviour.

	  Morse receiver will receive only first 256 chars. This is what Morse
	  code receiver will receive when this test tries to play a message with
	  count of chars that is larger than cwdaemon's receive buffer (the
	  receive buffer has space for 256 chars). The last character from
	  request will be dropped by cwdaemon's receive code.

	  Count of bytes in the sent request doesn't include terminating NUL.
	*/
	{ .description = "plain request with size larger than cwdaemon's receive buffer - 257 bytes (without NUL); TRUNCATION of Morse receive",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "1234567"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "123456",
	},
	/*
	  In this case only a truncated plain request is keyed on cwdevice and
	  received by Morse code receiver. Request's characters that won't fit
	  into cwdaemon's receive buffer will be dropped by cwdaemon's receive
	  code and won't be keyed on cwdevice.

	  This case could be described as "failure case" because Morse receiver
	  will return something else than what client has sent to cwdaemon
	  server. But we know that cwdaemon server will drop extra char(s) from
	  the plain request, and we know what cwdaemon server will key on
	  cwdevice. And this test case is expecting and testing exactly this
	  behaviour.

	  Morse receiver will receive only first 256 chars. This is what Morse
	  code receiver will receive when this test tries to play a message with
	  count of chars that is larger than cwdaemon's receive buffer (the
	  receive buffer has space for 256 chars). The last character from
	  request will be dropped by cwdaemon's receive code.

	  The count of bytes in the sent request includes terminating NUL.
	*/
	{ .description = "plain request with size larger than cwdaemon's receive buffer - 257+1 bytes (with NUL); TRUNCATION of Morse receive",
	  .plain_request          = SOCKET_BUF_SET("paris kukukukukukuku" CHARS_21_250 "1234567\0"),
	  .expected_morse_receive =                "paris kukukukukukuku" CHARS_21_250 "123456",
	},
};




int request_size_tests(void)
{
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	return run_test_cases(g_test_cases, n_test_cases);
}

