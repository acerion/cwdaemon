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
   @file Unit tests for cwdaemon's functions that handle command line
   options.
*/




#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "src/options.h"
#include "tests/library/log.h"




/*
  Global variables used by files compiled for this test. The variables are
  normally defined in cwdaemon's main file. For the purposes of the files
  linked in this test we need to define them here.
*/
FILE * cwdaemon_debug_f;
FILE * cwdaemon_debug_f_path;
bool g_forking;                /* false -> not forking -> logs from tested functions will be redirected to cwdaemon_debug_f. */
int current_verbosity = 3;     /* CWDAEMON_VERBOSITY_I */




static int test_option_network_port(void);




static int (*tests[])(void) = {
	test_option_network_port,
	NULL
};




int main(void)
{
	// cwdaemon_debug_f = stdout;

	int i = 0;
	while (tests[i]) {
		if (0 != tests[i]()) {
			fprintf(stdout, "[EE] Test result: failure in tests #%d\n", i);
			return -1;
		}
		i++;
	}

	fprintf(stdout, "[II] Test result: success\n");
	return 0;
}




static int test_option_network_port(void)
{
	const in_port_t doesnt_matter = 3333; /* Some value in the middle of valid range of port values. */

	/*
	  All these cases are valid cases. Tested function should succeed in
	  building *some* path. The path may represent a non-existing device, but
	  it will always be a valid string starting with "/dev/".
	*/
	const struct {
		const char * opt_value;
		bool expected_success;
		in_port_t expected_port;
	} test_data[] = {
		{ .opt_value =    "-1", .expected_success = false, .expected_port = doesnt_matter }, /* Negative value. */
		{ .opt_value =     "0", .expected_success = false, .expected_port = doesnt_matter }, /* This is probably not a valid port number at all. */
		{ .opt_value =     "1", .expected_success = false, .expected_port = doesnt_matter }, /* Privileged port, not in CWDAEMON's MIN-MAX range. The same for next few rows. */
		{ .opt_value =     "2", .expected_success = false, .expected_port = doesnt_matter },
		{ .opt_value =     "3", .expected_success = false, .expected_port = doesnt_matter },
		{ .opt_value =  "1021", .expected_success = false, .expected_port = doesnt_matter },
		{ .opt_value =  "1022", .expected_success = false, .expected_port = doesnt_matter },
		{ .opt_value =  "1023", .expected_success = false, .expected_port = doesnt_matter },

		{ .opt_value =  "1024", .expected_success = true,  .expected_port =  1024         },  /* CWDAEMON_NETWORK_PORT_MIN */
		{ .opt_value =  "1025", .expected_success = true,  .expected_port =  1025         },
		{ .opt_value =  "1026", .expected_success = true,  .expected_port =  1026         },
		{ .opt_value = "65533", .expected_success = true,  .expected_port = 65533         },
		{ .opt_value = "65534", .expected_success = true,  .expected_port = 65534         },
		{ .opt_value = "65535", .expected_success = true,  .expected_port = 65535         },  /* CWDAEMON_NETWORK_PORT_MAX */

		{ .opt_value = "65536", .expected_success = false, .expected_port = doesnt_matter },
		{ .opt_value = "65537", .expected_success = false, .expected_port = doesnt_matter },
		{ .opt_value = "65538", .expected_success = false, .expected_port = doesnt_matter },

		{ .opt_value =      "", .expected_success = false, .expected_port = doesnt_matter }, /* Empty value of option. */
		{ .opt_value = "paris", .expected_success = false, .expected_port = doesnt_matter }, /* Non-digits string. */
		{ .opt_value = "1024b", .expected_success = false, .expected_port = doesnt_matter }, /* Non-only-digits string. */
		{ .opt_value = "k1025", .expected_success = false, .expected_port = doesnt_matter }, /* String starting with non-digit. */
		{ .opt_value =  "ffff", .expected_success = false, .expected_port = doesnt_matter }, /* Hex digit, not acceptable for function expecting a decimal. */
	};


	const size_t n = sizeof (test_data) / sizeof (test_data[0]);
	for (size_t i = 0; i < n; i++) {

		in_port_t port = 0;
		const int retv = cwdaemon_option_network_port(&port, test_data[i].opt_value);
		if (test_data[i].expected_success) {
			if (0 != retv) {
				test_log_err("Tested function returns failure where a success was expected in test %zu / %zu, opt_value = [%s]\n",
				             i + 1, n, test_data[i].opt_value);
				return -1;
			}
			if (port != test_data[i].expected_port) {
				test_log_err("Tested function returns unexpected port value %u where %u was expected in test %zu / %zu, opt_value = [%s]\n",
				             port, test_data[i].expected_port,
				             i + 1, n, test_data[i].opt_value);
				return -1;
			}
		} else {
			if (0 == retv) {
				test_log_err("Tested function returns success where a failure was expected in test %zu / %zu, opt_value = [%s]\n",
				             i + 1, n, test_data[i].opt_value);
				return -1;
			}
		}
	}

	test_log_info("Tests of cwdaemon_option_network_port() have succeeded %s\n", "");

	return 0;
}




