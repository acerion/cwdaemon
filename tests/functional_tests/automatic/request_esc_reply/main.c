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
   Tests of "REPLY" escape request.
*/




#define _DEFAULT_SOURCE




#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "basic.h"
#include "request_size.h"

#include "tests/library/log.h"
#include "tests/library/random.h"




static int (*g_tests[])(void) = {
	basic_tests,
	//request_size_tests,
};




int main(void)
{
#if 0
	if (!test_env_is_usable(test_env_libcw_without_signals)) {
		test_log_err("Test: preconditions for test env are not met, exiting\n");
		exit(EXIT_FAILURE);
	}
#endif

	const uint32_t seed = cwdaemon_srandom(0);
	test_log_debug("Test: random seed: 0x%08x (%u)\n", seed, seed);

	bool failure = false;
	const size_t n_tests = sizeof (g_tests) / sizeof (g_tests[0]);

	for (size_t i = 0; i < n_tests; i++) {
		test_log_info("Test: running test %zu / %zu\n", i + 1, n_tests);
		if (0 != g_tests[i]()) {
			test_log_err("Test: test %zu / %zu has failed\n", i + 1, n_tests);
			failure = true;
			break;
		}
	}

	test_log_newline(); /* Visual separator. */

	if (failure) {
		test_log_err("Test: final result: FAIL %s\n", "");
		exit(EXIT_FAILURE);
	}

	test_log_info("Test: final result: PASS %s\n", "");
	exit(EXIT_SUCCESS);
}


