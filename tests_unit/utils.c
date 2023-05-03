/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2023 Kamil Ignacak <acerion@wp.pl>
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
	@file Unit tests for cwdaemon/src/utils.c
*/




#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "../src/utils.h"




static int test_build_full_device_path_success(void);
static int test_build_full_device_path_failure(void);
static int test_build_full_device_path_length(void);




static int (*tests[])(void) = {
	test_build_full_device_path_success,
	test_build_full_device_path_failure,
	test_build_full_device_path_length,
	NULL
};




int main(void)
{
	int i = 0;
	while (tests[i]) {
		if (0 != tests[i]()) {
			fprintf(stdout, "Test result: failure in tests #%d\n", i);
			return -1;
		}
		i++;
	}

	fprintf(stdout, "Test result: success\n");
	return 0;
}




/*
  Testing different success cases.
*/
static int test_build_full_device_path_success(void)
{
	char buffer[128] = { 0 };

	/*
	  All these cases are valid cases. Tested function should succeed in
	  building *some* path. The path may represent a non-existing device, but
	  it will always be a valid string starting with "/dev/".
	*/
	const struct {
		const char * input;
		int expected_retv;
		const char * expected_path;
	} test_data[] = {
		{ "/dev/ttyUSB0",            0, "/dev/ttyUSB0"       },
		{ "dev/ttyUSB0",             0, "/dev/dev/ttyUSB0"   },
		{ "ttyS0",                   0, "/dev/ttyS0"         },
		{ "/ttyS0",                  0, "/dev//ttyS0"        },
		{ "../..//ttyS0",            0, "/dev/../..//ttyS0"  },
	};


	for (size_t i = 0; i < sizeof (test_data) / sizeof (test_data[0]); i++) {
		int retv = build_full_device_path(buffer, sizeof (buffer), test_data[i].input);
		if (test_data[i].expected_retv != retv) {
			fprintf(stderr, "[EE] build_full_device_path(%s, %zd, %s) gives wrong return value %d/%s (success test #%zd)\n",
					  buffer, sizeof (buffer), test_data[i].input, retv, strerror(-retv), i);
			return -1;
		}

		if (0 != strcmp(buffer, test_data[i].expected_path)) {
			fprintf(stderr, "[EE] build_full_device_path(%s, %zd, %s) gives wrong result [%s] (success test #%zd)\n",
					  buffer, sizeof (buffer), test_data[i].input, buffer, i);
			return -1;
		}
	}

	return 0;
}




/*
  Testing different failure cases.
*/
static int test_build_full_device_path_failure(void)
{
	char small_buffer[4]; /* Buffer too small to fit a result. */
	char big_buffer[20];  /* Buffer that is big enough to fit a result. */
	const struct {
		const char * input;
		char * buffer;
		size_t buffer_size;
		int expected_retv;
	} test_data[] = {
		{ "/dev/null",  small_buffer, sizeof (small_buffer),   -ENAMETOOLONG },  /* Input is a too long device path. */
		{ "null",       small_buffer, sizeof (small_buffer),   -ENAMETOOLONG },  /* Input is a too long device name. */
		{ NULL,         big_buffer,   sizeof (big_buffer),     -EINVAL       },  /* Invalid 'input' arg. */
		{ "null",       NULL,         sizeof (big_buffer),     -EINVAL       },  /* Invalid 'result' arg. */
		{ "null",       big_buffer,   0,                       -EINVAL       },  /* Invalid 'size' arg. */
	};

	for (size_t i = 0; i < sizeof (test_data) / sizeof (test_data[0]); i++) {
		int retv = build_full_device_path(test_data[i].buffer, test_data[i].buffer_size, test_data[i].input);
		if (test_data[i].expected_retv != retv) {
			fprintf(stderr, "[EE] build_full_device_path(%s, %zd, %s) gives wrong return value %d/%s (failure test #%zd)\n",
					  test_data[i].buffer, test_data[i].buffer_size, test_data[i].input, retv, strerror(-retv), i);
			return -1;
		}
	}

	return 0;
}




/*
  Tests designed specifically to check for correct handling of too long input.
*/
static int test_build_full_device_path_length(void)
{
	char a_buffer[10]; /* If you change size of this buffer, you will need to change test data too. */
	const struct {
		const char * input;
		char * buffer;
		size_t buffer_size;
		int expected_retv;
		const char * expected_result;
	} test_data[] = {
		{ "/dev/null",  a_buffer, sizeof (a_buffer),   0, "/dev/null" },  /* Input path has 9 characters, it will fit into a_buffer. */
		{ "null",       a_buffer, sizeof (a_buffer),   0, "/dev/null" },  /* Input name + added prefix will give 9 characters, it will fit into a_buffer. */

		{ "/dev/null2", a_buffer, sizeof (a_buffer),   -ENAMETOOLONG, "/dev/null" },  /* Input path has 10 characters, it will NOT fit into a_buffer - it will be truncated. */
		{ "null3",      a_buffer, sizeof (a_buffer),   -ENAMETOOLONG, "/dev/null" },  /* Input name + added prefix will give 10 characters, it will NOT fit into a_buffer - it will be truncated. */
	};

	for (size_t i = 0; i < sizeof (test_data) / sizeof (test_data[0]); i++) {
		int retv = build_full_device_path(test_data[i].buffer, test_data[i].buffer_size, test_data[i].input);
		if (test_data[i].expected_retv != retv) {
			fprintf(stderr, "[EE] build_full_device_path(%s, %zd, %s) gives wrong return value %d/%s (length test #%zd)\n",
					  test_data[i].buffer, test_data[i].buffer_size, test_data[i].input, retv, strerror(-retv), i);
			return -1;
		}
		if (0 != strcmp(test_data[i].buffer, test_data[i].expected_result)) {
			fprintf(stderr, "[EE] build_full_device_path(%s, %zd, %s) gives wrong result [%s] (length test #%zd)\n",
					  test_data[i].buffer, test_data[i].buffer_size, test_data[i].input, test_data[i].buffer, i);
			return -1;
		}
	}

	return 0;
}

