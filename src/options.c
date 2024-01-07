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
 * GNU Library General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   Code parsing values of cwdaemon's command line options.
*/




#include "src/cwdaemon.h"
#include "src/log.h"
#include "src/options.h"
#include "src/utils.h"




/**
   @brief Parse value of "-p"/"--port" command line option

   @param[out] port Parsed value of network port
   @param[in] opt_value Value of command line option

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




