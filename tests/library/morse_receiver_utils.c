/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   @file

   Functions related to Morse receiver. The functions are put in separate
   file to make unit testing easier. The functions have very few
   dependencies, so unit tests don't have to pull many dependencies either.
*/




//#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "tests/library/log.h"
#include "tests/library/morse_receiver_utils.h"




bool morse_receive_text_is_correct(const char * received_text, const char * expected_message)
{
	/*
	  When comparing strings, remember that a libcw receiver may have
	  received first characters incorrectly (probably due to some bug in the
	  receiver). This function disregards differences in first letters of its
	  arguments.

	  TODO acerion 2024.01.27: the function needs some improvements. The
	  function should confirm that the expected_message is at the very end of
	  received text.

	  TODO acerion 2024.01.28: review the function once the receiver is
	  improved to the point where it doesn't make mistakes at the beginning
	  of string.
	*/

	size_t received_len = strlen(received_text);
	size_t expected_len = strlen(expected_message);

	/* Receiver will usually add an inter-word-space at the end. Skip it. */
	while (received_len > 0 && isspace(received_text[received_len - 1])) {
		received_len--;
	}
	/* Skip white spaces at end of the other string too, while we are at it. */
	while (expected_len > 0 && isspace(expected_message[expected_len - 1])) {
		expected_len--;
	}

	if (received_len < expected_len) {
		/* If we received less than expected, then it's most probably an
		   error in cwdaemon or in testing procedure. */
		test_log_err("Morse receiver: received string is incorrect: inacceptable difference in lengths: [%s]/[%s]\n",
		             received_text, expected_message);
		return false;
	}

	/* The condition in the loop is 'idx >= 1' on purpose. The loop doesn't go
	   to 0th character because current implementation of cw receiver
	   incorrectly receives first character. Therefore we skip it, until the
	   receiver is fixed. TODO acerion 2024.01.27: change the value in
	   condition to zero once the receiver is fixed. */
	size_t r_idx = received_len - 1;
	size_t e_idx = expected_len - 1;
	for (; r_idx >= 1 && e_idx >= 1; r_idx--, e_idx--) {
		const char r_char = received_text[r_idx];
		const char e_char = expected_message[e_idx];
		if (tolower((unsigned char) r_char) != tolower((unsigned char) e_char)) {
			test_log_err("Morse receiver: mismatch at positions %zu/%zu in [%s]/[%s]: '%c' != '%c'\n",
			             r_idx, e_idx, received_text, expected_message, r_char, e_char);
			return false;
		}
		if (0) { /* Debug. */
			test_log_debug("Morse receiver: match at positions %zu/%zu in [%s]/[%s]: '%c' == '%c'\n",
			               r_idx, e_idx, received_text, expected_message, r_char, e_char);
		}
	}

	return true;
}




