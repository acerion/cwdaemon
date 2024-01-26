/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
 * Copyright (C) 2012 - 2023 Kamil Ignacak <acerion@wp.pl>
 *
 * Some of this code is taken from netkeyer.c, which is part of the tlf source,
 * here is the copyright:
 * Tlf - contest logging program for amateur radio operators
 * Copyright (C) 2001-2002-2003 Rein Couperus <pa0rct@amsat.org>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   Socket functions for cwdaemon tests
*/




#define _POSIX_C_SOURCE 200112L /* getaddrinfo() and friends. */

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>




#include "socket.h"




int open_socket_to_server(const char * server_ip_address, in_port_t server_in_port)
{
	/* Code in this function has been copied from
	   getaddrinfo() man page. */

	struct addrinfo hints = { 0 };
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = IPPROTO_UDP;

	struct addrinfo * result = NULL;
	char port_buf[16] = { 0 };
	snprintf(port_buf, sizeof (port_buf), "%d", server_in_port);
	int rv = getaddrinfo(server_ip_address, port_buf, &hints, &result);
	if (rv) {
		fprintf(stderr, "[EE] call to getaddrinfo() failed %s\n", gai_strerror(rv));
		return -1;
	}

	/* getaddrinfo() returns a list of address structures. Try each address
	   until we successfully connect(2). If socket(2) or connect(2) fails, we
	   close the socket and try the next address. */
	int fd = -1;
	for (struct addrinfo * rp = result; rp; rp = rp->ai_next) {
		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd == -1) {
			continue;
		}

		if (connect(fd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}

		close(fd);
		fd = -1;
	}
	freeaddrinfo(result); /* No longer needed */

	if (-1 == fd) {
		fprintf(stderr, "[EE] Cannot not open a socket to cwdaemon server\n");
	}
	return fd;
}




