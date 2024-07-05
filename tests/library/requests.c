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




#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/cwdaemon.h"
#include "log.h"
#include "random.h"
#include "requests.h"
#include "string_utils.h"




/// @file
///
/// Code for building requests sent to cwdaemon server.




// TODO (acerion) 2024.07.05: replace 'int mode' with
// 'value_generation_mode_t mode'.
static int get_value_mode(int * mode, tests_value_generation_probabilities_t const * percentages)
{
	unsigned int const mode_lower = 1;
	unsigned int const mode_upper = 100;
	unsigned int mode_val = 0;

	if (0 != cwdaemon_random_uint(mode_lower, mode_upper, &mode_val)) {
		test_log_err("Test: failed to generate mode of getting a port value %s\n", "");
		return -1;
	}

	unsigned int const thresh_valid = percentages->valid;
	unsigned int const thresh_empty = thresh_valid + percentages->empty;
	unsigned int const thresh_invalid = thresh_empty + percentages->invalid;
	unsigned int const thresh_random_bytes = thresh_invalid + percentages->random_bytes;

	if (mode_val <= thresh_valid) { // Proper port value.
		*mode = 0;
	} else if (mode_val <= thresh_empty) { // Empty string
		*mode = 1;
	} else if (mode_val <= thresh_invalid) { // Invalid port number.
		*mode = 2;
	} else if (mode_val <= thresh_random_bytes) { // Put random bytes into request.
		*mode = 3;
	} else {
		test_log_info("Test: mode value %u not caught by percentages\n", mode_val);
		return -1;
	}

	return 0;
}




/// @brief Build a PORT Escape request
///
/// Fill given @p request with bytes that make a proper PORT Escape request.
/// Put there a value that will try to make cwdaemon switch to new network
/// port.
///
/// The value (an array of bytes) is or is not terminated by NUL - this is
/// decided at random. cwdaemon server should be able to safely handle both
/// cases.
///
/// @p request->n_bytes is set according to count of bytes (with or without
/// NUL) put into the request.
///
/// TODO (acerion) 2024.07.05: this function should use
/// test_request_set_value_int() from tests/fuzzing/simple/main.c. Also use
/// value_generation_mode_t from that file.
///
/// @reviewed_on{2024.07.05}
///
/// @param[out] request PORT Escape request built by this function
/// @param[out] percentages Probabilities of generating value in different ways
///
/// @return 0 when request was built without problems
/// @return -1 otherwise
int tests_requests_build_request_esc_port(test_request_t * request, tests_value_generation_probabilities_t const * percentages)
{
	int mode = 0;
	if (0 != get_value_mode(&mode, percentages)) {
		test_log_err("Test: failed to get 'value generation mode' for PORT Escape request %s", "");
		return -1;
	}

	// String with request's value. -1 for ESC char and -1 for request code.
	char value_string[CLIENT_SEND_BUFFER_SIZE - 2] = { 0 };

	// Generate string with request's value.
	switch (mode) {
	case 0:
		{ // Proper port value.
			test_log_info("Test: generating request string with VALID parameter value %s\n", "");

			// Valid range of port numbers.
			unsigned int const lower = 0;
			unsigned int const upper = 65535;
			unsigned int val = 0;

			if (0 != cwdaemon_random_uint(lower, upper, &val)) {
				test_log_err("Test: failed to generate random valid value of port %s\n", "");
				return -1;
			}
			snprintf(value_string, sizeof (value_string), "%u", (in_port_t) val);
		}
		break;

	case 1:// Empty string
		test_log_info("Test: generating request string with EMPTY parameter value %s\n", "");
		// value_string[] is already initialized with zeros.
		break;

	case 2: // Invalid port number.
		test_log_info("Test: generating request string with INVALID parameter value %s\n", "");
		// TODO (acerion) 2024.07.04: be more creative.
		snprintf(value_string, sizeof (value_string), "%d", -1);
		break;

	case 3: // Put random bytes into request.
		// TODO (acerion) 2024.07.04: implement this.
		test_log_err("Test: Generating RANDOM BYTES for PORT Escape request is not supported yet %s\n", "");
		return -1;

	default:
		test_log_info("Test: mode value %d not caught by percentages\n", mode);
		return -1;
	}

	// Count of bytes in value.
	size_t val_n_bytes = strlen(value_string);
	bool with_nul = false;
	if (0 != cwdaemon_random_bool(&with_nul)) {
		test_log_err("Test: failed to decide if we want to append terminating NUL to PORT port string %s\n", "");
		return -1;
	}
	if (with_nul) {
		// Also include terminating NUL in a port string sent in the request.
		val_n_bytes++;
	}

	// Build request's bytes and n_bytes.
	request->n_bytes = 0;
	request->bytes[request->n_bytes++] = ASCII_ESC;
	request->bytes[request->n_bytes++] = CWDAEMON_ESC_REQUEST_PORT;
	for (size_t i_val = 0; i_val < val_n_bytes; i_val++) {
		request->bytes[request->n_bytes++] = value_string[i_val];
	}

#if 1 // Only for debug.
	char printable[PRINTABLE_BUFFER_SIZE(sizeof (request->bytes))] = { 0 };
	get_printable_string(request->bytes, request->n_bytes, printable, sizeof (printable));
	test_log_debug("Test: Generated %zu bytes of PORT Escape request: [%s]\n", request->n_bytes, printable);
#endif

	return 0;
}



