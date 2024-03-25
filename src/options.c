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




/**
   Code parsing values of cwdaemon's command line options.
*/



#include <string.h>

#include "src/cwdaemon.h"
#include "src/log.h"
#include "src/options.h"
#include "src/utils.h"




/**
   @brief Parse value of "-p"/"--port" command line option

   @param[out] port Parsed value of network port
   @param[in] opt_value String value of command line option

   @return 0 on success
   @return -1 on failure
*/
int cwdaemon_option_network_port(in_port_t * port, const char * opt_value)
{
	const long int port_min = CWDAEMON_NETWORK_PORT_MIN;
	const long int port_max = CWDAEMON_NETWORK_PORT_MAX;
	long lv = 0;
	if (!cwdaemon_get_long(opt_value, &lv) || lv < port_min || lv > port_max) {
		log_message(LOG_ERR, "Invalid requested port number: \"%s\", must be in range <%ld - %ld>, inclusive",
		            opt_value, port_min, port_max);
		return -1;
	}

	*port = (in_port_t) lv;
	log_message(LOG_INFO, "requested port number: %u", *port);
	return 0;
}




/**
   @brief Parse value of "-I"/"--libcwflags" command line option

   @param[out] flags Parsed value of flags
   @param[in] opt_value String value of command line option

   @return 0 on success
   @return -1 on failure
*/
int cwdaemon_option_libcwflags(uint32_t * flags, const char * opt_value)
{
	long lv = 0;
	if (!cwdaemon_get_long(opt_value, &lv)) {
		log_message(LOG_ERR, "Invalid requested debug flags: \"%s\" (should be decimal value)", opt_value);
		*flags = 0;

		return -1;
	}

	*flags = (uint32_t) lv;
	log_message(LOG_INFO, "Requested libcw debug flags: %u (dec) / %08x (hex)", *flags, *flags);
	return 0;
}




void cwdaemon_option_inc_verbosity(int * threshold)
{
	if (*threshold < LOG_DEBUG) {
		(*threshold)++;

		log_info("requested log threshold: \"%s\"", log_get_priority_label(*threshold));
	}

	return;
}




int cwdaemon_option_set_verbosity(int * threshold, const char * opt_arg)
{
	if (NULL == opt_arg) {
		log_error("Invalid arg while setting log threshold %s", "");
		return -1;
	}

	if (!strcmp(opt_arg, "n") || !strcmp(opt_arg, "N")) { /* In cwdaemon 'N' means 'None', i.e. "threshold so high that nothing gets logged". */
		*threshold = LOG_CRIT;
	} else if (!strcmp(opt_arg, "e") || !strcmp(opt_arg, "E")) {
		*threshold = LOG_ERR;
	} else if (!strcmp(opt_arg, "w") || !strcmp(opt_arg, "W")) {
		*threshold = LOG_WARNING;
	} else if (!strcmp(opt_arg, "i") || !strcmp(opt_arg, "I")) {
		*threshold = LOG_INFO;
	} else if (!strcmp(opt_arg, "d") || !strcmp(opt_arg, "D")) {
		*threshold = LOG_DEBUG;
	} else {
		log_error("invalid requested log threshold: \"%s\"", opt_arg);
		return -1;
	}

	log_info("requested log threshold: \"%s\"", log_get_priority_label(*threshold));
	return 0;
}




