/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
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




/// @file
///
/// Code parsing values of cwdaemon's command line options.




#include <ctype.h>
#include <string.h>

#include "cwdaemon.h"
#include "log.h"
#include "options.h"
#include "utils.h"




// @reviewed_on{2024.05.10}
int cwdaemon_option_network_port(in_port_t * port, char const * opt_value)
{
	const long int port_min = CWDAEMON_NETWORK_PORT_MIN;
	const long int port_max = CWDAEMON_NETWORK_PORT_MAX;
	long lv = 0;
	if (!cwdaemon_get_long(opt_value, &lv) || lv < port_min || lv > port_max) {
		log_error("Invalid requested port number: \"%s\", must be in range <%ld - %ld>, inclusive",
		          opt_value, port_min, port_max);
		return -1;
	}

	*port = (in_port_t) lv;
	log_info("Requested port number: %u", *port);
	return 0;
}




// @reviewed_on{2024.05.10}
int cwdaemon_option_libcwflags(uint32_t * flags, char const * opt_value)
{
	long lv = 0;
	if (!cwdaemon_get_long(opt_value, &lv)) {
		log_error("Invalid requested debug flags: \"%s\" (should be decimal value)", opt_value);
		return -1;
	}

	*flags = (uint32_t) lv;
	log_info("Requested libcw debug flags: %u (dec) / %08x (hex)", *flags, *flags);
	return 0;
}




// @reviewed_on{2024.05.10}
void cwdaemon_option_inc_verbosity(int * threshold)
{
	if (*threshold < LOG_DEBUG) {
		(*threshold)++;

		log_info("Requested log threshold: \"%s\"", log_get_priority_label(*threshold));
	}

	return;
}




// @reviewed_on{2024.05.10}
int cwdaemon_option_set_verbosity(int * threshold, char const * opt_value)
{
	if (NULL == opt_value) {
		log_error("Invalid arg while setting log threshold %s", "");
		return -1;
	}
	if ('\0' == opt_value[0]) {
		log_error("Empty value of log threshold option %s", "");
		return -1;
	}

	char const c = (char) tolower((int) opt_value[0]);
	switch (c) {
	case 'n':
		// In cwdaemon 'n' means 'None'. Set threshold so high that nothing
		// gets logged. cwdaemon doesn't use LOG_CRIT priority in any of its
		// logs.
		*threshold = LOG_CRIT;
		break;
	case 'e':
		*threshold = LOG_ERR;
		break;
	case 'w':
		*threshold = LOG_WARNING;
		break;
	case 'i':
		*threshold = LOG_INFO;
		break;
	case 'd':
		*threshold = LOG_DEBUG;
		break;
	default:
		log_error("Invalid requested log threshold: \"%s\"", opt_value);
		return -1;
	}

	log_info("Requested log threshold: \"%s\"", log_get_priority_label(*threshold));
	return 0;
}




// @reviewed_on{2024.05.12}
int cwdaemon_option_cwdevice(cwdevice ** device, char const * opt_value)
{
	if (!opt_value || !strlen(opt_value)) {
		log_error("Invalid cwdevice name/path [%s]", opt_value);
		return -1;
	}

	if (!cwdaemon_cwdevice_set(device, opt_value)) {
		log_error("Unrecognized requested cwdevice [%s]", opt_value);
		return -1;
	}

	log_info("Requested cwdevice [%s]", opt_value);
	return 0;
}


