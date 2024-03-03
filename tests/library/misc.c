/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
 * Copyright (C) 2012 - 2024 Kamil Ignacak <acerion@wp.pl>
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




#include "config.h"
#include "test_defines.h"

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

#include "client.h"
#include "log.h"
#include "misc.h"
#include "random.h"
#include "socket.h"
#include "src/cwdaemon.h"




/**
   @brief Test if give local Layer 4 port is used (open) or not

   @param[in] port port number to test

   @return true if given port is used (open), or if due to error this cannot be checked
   @return false otherwise
*/
static bool is_local_udp_port_used(in_port_t port)
{
	struct sockaddr_in request_addr = { 0 };
	request_addr.sin_family = AF_INET;
	request_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	request_addr.sin_port = htons(port);
	socklen_t request_addrlen = sizeof (request_addr);

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		test_log_err("Test: can't open socket %s\n", "");
		 /* Since we can't open a socket we can't be really sure, but
		    to be safe return true here. */
		return true;
	}

	int b = bind(fd, (struct sockaddr *) &request_addr, request_addrlen);
	close(fd);
	return -1 == b;
}




int find_unused_random_biased_local_udp_port(in_port_t * port)
{
	/*
	  Be slightly biased towards selecting cwdaemon's default port.

	  This bias is needed to:
	  - make situations where test code doesn't explicitly specify port
	    option to be more frequent. Test code can recognize that a default
	    port is requested, and the test code may then decide to not specify
	    port explicitly. Not specifying port explicitly and relying on
	    implicit/default port number is just another case of functional
	    testing.
	  - make situations where cwdaemon is tested with its most commonly used
	    port number more frequent. cwdaemon can be started with any
	    unprivileged port, but I believe that in 99.9% of situations it is
	    listening on default port.

	  Look for following logs in output of functional tests that are a
	  result of the bias:

	  "
	  [II] cwdaemon will start with default port, without explicit 'port' option
	  /home/kamil/tmp/tmpfs/git_repo/src/cwdaemon -T 640 -x s -n -d ttyUSB0 -s 10
	  "

	  TODO acerion 2024.01.07: can we somehow test automatically the fact
	  that sometimes the port is not specified explicitly?
	*/
	unsigned int bias_input = 0;
	const unsigned int bias_min = 0;
	const unsigned int bias_max = 20;
	if (0 != cwdaemon_random_uint(bias_min, bias_max, &bias_input)) {
		return -1;
	}
	const bool try_default_port_first = bias_input == bias_max;


	const int n = 1000; /* We should be able to find some unused port in 1000 tries, right? */
	for (int i = 0; i < n; i++) {
		unsigned int value = 0;

		if (i == 0 && try_default_port_first) {
			value = CWDAEMON_NETWORK_PORT_DEFAULT;
		} else {
			const unsigned int lower = CWDAEMON_NETWORK_PORT_MIN;
			const unsigned int upper = CWDAEMON_NETWORK_PORT_MAX;
			if (0 != cwdaemon_random_uint(lower, upper, &value)) {
				test_log_err("Test: failed to get random port in range %u - %u\n", lower, upper);
				return -1;
			}
		}

		if (!is_local_udp_port_used((in_port_t) value)) {
			*port = (in_port_t) value;
			return 0;
		}
	}

	return -1;
}




/*
  Alternative implementation of function looking for unused port. May work
  with remote machines too (but I didn't test it all that well).
*/
__attribute__((unused))
static bool is_remote_port_open_by_cwdaemon(const char * server, in_port_t server_in_port)
{
	struct timeval tv = { .tv_sec = 2 };

	int socket = open_socket_to_server(server, server_in_port);
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof (tv));

	const char * requested_message_value = "e";
	const char * requested_reply_value   = "t";

	client_t client = { .sock = socket };
	client_send_esc_request(&client, CWDAEMON_ESC_REQUEST_REPLY, requested_reply_value, strlen(requested_reply_value) + 1);
	client_send_message(&client, requested_message_value, strlen(requested_message_value) + 1);

	/* Try receiving preconfigured reply. Receiving it means that there
	   is a process on the other side of socket that behaves like
	   cwdaemon. */
	char recv_buf[CLIENT_RECV_BUFFER_SIZE] = { 0 };
	const ssize_t r = recv(socket, recv_buf, sizeof (recv_buf), 0);
	close(socket);

	// TODO (acerion): we should compare recv_buf with requested_reply_value.

	test_log_debug("Test: trying to communicate with remote server at [%s:%u], recv(%d) -> %ld\n", server, server_in_port, socket, r);

	return -1 != r;
}




int test_get_test_wpm(void)
{
	int wpm = 0;
	/* Remember that some receive timeouts in tests were selected when the
	   wpm was hardcoded to 10 wpm. Picking values lower than 10 may lead to
	   overrunning the timeouts. */
	if (0 != cwdaemon_random_uint(TEST_WPM_MIN, TEST_WPM_MAX, (unsigned int *) &wpm)) {
		wpm = TEST_WPM_DEFAULT;
	}

	return wpm;
}

