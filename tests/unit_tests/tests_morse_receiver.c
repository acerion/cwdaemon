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

   Unit tests for tests/library/morse_receiver.c
*/




#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "tests/library/log.h"
#include "tests/library/morse_receiver_utils.h"




static int test_morse_receive_text_is_correct(void);




static int (*g_tests[])(void) = {
	test_morse_receive_text_is_correct,
	NULL
};




int main(void)
{
	int i = 0;
	while (g_tests[i]) {
		if (0 != g_tests[i]()) {
			fprintf(stderr, "[EE] Test result: failure in test #%d\n", i);
			return -1;
		}
		i++;
	}

	fprintf(stdout, "[II] Test result: success\n");
	return 0;
}




/*
  The function talks about receiver's mistakes. Currently Morse receiver may
  incorrectly receive first letter. Sadly this is currently accepted.

  TODO acerion 2024.01.08 Review the tests once the receiver is improved.
*/
static int test_morse_receive_text_is_correct(void)
{
	const struct {
		const char * message;       /* What client wanted to send as Morse code. */
		const char * received_text; /* What Morse receiver received. */
		bool expected_correct;      /* Should the two above strings be considered equal? */
	} test_data[] = {
		/* Success case. Basic, clear case without any mistake from receiver. */
		{ .message       = "Hello",
		  .received_text = "Hello",
		  .expected_correct = true, },

		/* Success case. Basic, clear case with acceptable mistake from receiver */
		{ .message       = "Hello",
		  .received_text = "Wello",
		  .expected_correct = true, },

		/* Success case. Basic, clear case without any mistake from receiver. Short string */
		{ .message       = "x1",
		  .received_text = "x1",
		  .expected_correct = true, },

		/* Success case. Received string has space at the end. */
		{ .message       = "Hello",
		  .received_text = "Wello ",
		  .expected_correct = true, },

		/* Success case. Message has space at the end. */
		{ .message       = "Hello ",
		  .received_text = "Wello",
		  .expected_correct = true, },

		/* Success case. Both strings have space at the end. */
		{ .message       = "Hello ",
		  .received_text = "Wello ",
		  .expected_correct = true, },

		/* Success case. More complicated strings. */
		{ .message       = "This is string, after all! \t",
		  .received_text = "Fhis is string, after all!\t ",
		  .expected_correct = true, },

		/* Failure case - a mistake of receiver at the beginning of string was too large. */
		{ .message       = "Hello",
		  .received_text = "ello",
		  .expected_correct = false, },

		/* Failure case - obvious case where received text is clearly wrong. */
		{ .message       = "Hello world",
		  .received_text = "Hello worlt",
		  .expected_correct = false, },

		/* Failure case - obvious case where received text is clearly wrong. */
		{ .message       = "Hello world",
		  .received_text = "Hello world!",
		  .expected_correct = false, },

		/* Failure case - obvious case where received text is clearly wrong. */
		{ .message       = "Hello world!",
		  .received_text = "Hello world",
		  .expected_correct = false, },

		/* Failure case - obvious case where received text is clearly wrong. */
		{ .message       = "Hello world!",
		  .received_text = "Hello world\t!",
		  .expected_correct = false, },
	};

	const size_t n_tests = sizeof (test_data) / sizeof (test_data[0]);
	for (size_t i = 0; i < n_tests; i++) {
		const bool correct = morse_receive_text_is_correct(test_data[i].received_text, test_data[i].message);
		if (correct != test_data[i].expected_correct) {
			test_log_err("Mismatch in function's return value in test #%zu / %zu: %s != %s\n",
			             i + 1, n_tests,
			             correct ? "true" : "false",
			             test_data[i].expected_correct ? "true" : "false");
			return -1;
		}
	}

	return 0;
}

