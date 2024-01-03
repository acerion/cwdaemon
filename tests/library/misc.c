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
   Misc helper functions for cwdaemon tests
*/



#define _GNU_SOURCE /* strcasestr() */

#include "config.h"

#if HAVE_NETINET_IN_H
#include <netinet/in.h> /* "struct sockaddr_in" in FreeBSD 13.2 */
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <libcw.h>

#include "misc.h"
#include "socket.h"
#include "src/lib/random.h"




/**
   @brief Test if give local Layer 4 port is used (open) or not

   @param[in] port port number to test

   @return true if given port is used (open), or if due to error this cannot be checked
   @return false otherwise
*/
static bool is_local_udp_port_used(int port)
{
	struct sockaddr_in request_addr = { 0 };
	request_addr.sin_family = AF_INET;
	request_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	request_addr.sin_port = htons(port);
	socklen_t request_addrlen = sizeof (request_addr);

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		fprintf(stderr, "[EE] can't open socket\n");
		 /* Since we can't open a socket we can't be really sure, but
		    to be safe return true here. */
		return true;
	}

	int b = bind(fd, (struct sockaddr *) &request_addr, request_addrlen);
	close(fd);
	return -1 == b;
}




/*
  TODO acerion 2022.02.18: this function should probably be slightly biased
  towards cwdaemon's default port. With the bias we can test the most common
  setup more often, while still being able to test uncommon cases.
*/
int find_unused_random_local_udp_port(void)
{
	const unsigned int lower = 1024;
	const unsigned int upper = 65535;

	const int n = 1000; /* We should be able to find some unused port in 1000 tries, right? */
	for (int i = 0; i < n; i++) {
		unsigned int port = 0;
		if (0 != cwdaemon_random_uint(lower, upper, &port)) {
			fprintf(stderr, "[ERROR] Failed to get port in range %d - %d\n", lower, upper);
			return 0;
		}

		if (!is_local_udp_port_used((int) port)) {
			return (int) port;
		}
	}
	return 0;
}




/*
  Alternative implementation of function looking for unused port. May work
  with remote machines too (but I didn't test it all that well).
*/
__attribute__((unused))
static bool is_remote_port_open_by_cwdaemon(const char * server, int port)
{
	struct timeval tv = { .tv_sec = 2 };

	char port_buf[16] = { 0 };
	snprintf(port_buf, sizeof (port_buf), "%d", port);
	int fd = cwdaemon_socket_connect(server, port_buf);
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof (tv));

	const char * requested_message_value = "e";
	const char * requested_reply_value   = "t";

	client_t client = { .sock = fd };
	client_send_request(&client, CWDAEMON_REQUEST_REPLY, requested_message_value);
	client_send_request(&client, CWDAEMON_REQUEST_MESSAGE, requested_reply_value);

	/* Try receiving preconfigured reply. Receiving it means that there
	   is a process on the other side of socket that behaves like
	   cwdaemon. */
	char recv_buf[32] = { 0 };
	const ssize_t r = recv(fd, recv_buf, sizeof (recv_buf), 0);
	close(fd);

	// TODO (acerion): we should compare recv_buf with requested_reply_value.

	//fprintf(stderr, "port %d, socket %d, rec %d\n", port, fd, r);

	return -1 != r;
}




char * escape_string(const char * buffer, char * escaped, size_t size)
{
	size_t e_idx = 0;

	for (size_t i = 0; i < strlen(buffer); i++) {
		switch (buffer[i]) {
		case '\r':
			escaped[e_idx++] = '\'';
			escaped[e_idx++] = 'C';
			escaped[e_idx++] = 'R';
			escaped[e_idx++] = '\'';
			break;
		case '\n':
			escaped[e_idx++] = '\'';
			escaped[e_idx++] = 'L';
			escaped[e_idx++] = 'F';
			escaped[e_idx++] = '\'';
			break;
		default:
			escaped[e_idx++] = buffer[i];
			break;
		}
	}

	escaped[size - 1] = '\0';
	return escaped;
}



