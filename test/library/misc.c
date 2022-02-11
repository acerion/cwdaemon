/*
 * misc.c - misc functions for cwdaemon tests
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
 * Copyright (C) 2012 - 2022 Kamil Ignacak <acerion@wp.pl>
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

#define _GNU_SOURCE /* strcasestr() */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "process.h"
#include "socket.h"
#include "misc.h"
#include "cw_rec_utils.h"


static int receive_from_key_source(int fd, cw_easy_receiver_t * easy_rec, char * buffer, size_t size, const char * expected_reply);


static void send_message_request_to_cwdaemon(int fd, cwdaemon_request_t * request)
{
	/* Ask cwdaemon to send us this reply back after playint text, so
	   that we don't wait in receive_from_key_source() for longer than
	   it's necessary to play and key requested text. */
	cwdaemon_socket_send_request(fd, CWDAEMON_REQUEST_REPLY, request->requested_reply);

	char value[64] = { 0 };
	snprintf(value, sizeof (value), "start %s", request->value);
	cwdaemon_socket_send_request(fd, request->id, value);
}




int cwdaemon_request_message_and_receive(cwdaemon_process_t * child, cwdaemon_request_t * request, cw_easy_receiver_t * easy_rec)
{
	char receive_buffer[30] = { 0 };
	int result = 0;

	/* When comparing strings, remember that a receiver may have received
	   first characters incorrectly. receive_from_key_source() prefixes a
	   text request with some startup text that is allowed to be
	   mis-received, so that the main part of text request is received
	   correctly and can be recognized with strcasestr(). */

	/* This sends a text request to cwdaemon that works in initial state,
	   i.e. reset command was not sent yet, so cwdaemon should not be
	   broken yet. */


	char expected_reply[64] = { 0 };
	/* Notice the initial 'h'. Notice terminating "\r\n". */
	snprintf(expected_reply, sizeof (expected_reply), "h%s\r\n", request->requested_reply);

	send_message_request_to_cwdaemon(child->fd, request);
	result += receive_from_key_source(child->fd, easy_rec, receive_buffer, sizeof (receive_buffer), expected_reply);
	if (!strcasestr(receive_buffer, request->value)) {
		result--;
	}

	return result;
}





/**
   Our key source is DTR pin on serial line.

   The pin is changed by cwdaemon.

   Changes of the pin are polled by key source. Key source calls
   `new_key_state_cb` callback every time it detects change of the pin, i.e.
   change of key state.

   @p easy_rec is notified on each change of the key state.

   Here we poll @p easy_rec receiver to see what it received.

   @param[in] fd socket to cwdaemon
   @param easy_rec easy receiver
   @param[out] buffer output buffer where received text will be put
   @param[in] size size of @p buffer
*/
static int receive_from_key_source(int fd, cw_easy_receiver_t * easy_rec, char * buffer, size_t size, const char * expected_reply)
{
	memset(buffer, 0, size);
	int buffer_i = 0;

	/* Loop countdown iterator as fallback in case if receiving a
	   preconfigured reply fails for some reason. */
	int loop_iters = 2000;

	do {
		usleep(10 * 1000); /* 10 milliseconds. TODO 2022.01.26: use a constant. TODO: use usleep from libcw.h */
		loop_iters--;
		if (0 == loop_iters) {
			fprintf(stderr, "[NN] Expected reply not received, leaving %s after loop countdown completed\n", __func__);
		}

		cw_rec_data_t erd = { 0 };
		if (!cw_easy_receiver_poll_data(easy_rec, &erd)) {
			continue;
		}

		if (erd.is_iws) {
			fprintf(stderr, " ");
			fflush(stderr);
			buffer[buffer_i++] = ' ';
		} else if (erd.character) {
			fprintf(stderr, "%c", erd.character);
			fflush(stderr);
			buffer[buffer_i++] = erd.character;
		} else {
			; /* NOOP */
		}


		/* Try receiving preconfigured reply. Receiving it means we
		   don't have to poll key for new key events because there
		   will be no more key events to receive (cwdaemon has
		   completed toggling tty pin). */
		char recv_buf[32] = { 0 };
		const int r = recv(fd, recv_buf, sizeof (recv_buf), MSG_DONTWAIT);
		if (-1 != r && 0 == strcmp(recv_buf, expected_reply)) {
			loop_iters = 0;
		}
	} while (loop_iters > 0);

	return 0;
}




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




int find_unused_random_local_udp_port(void)
{
	const int lower = 1024;
	const int upper = 65535;

	int n = 1000; /* We should be able to find some unused port in 1000 tries, right? */
	for (int i = 0; i < n; i++) {
		int port = rand();
		port %= ((upper + 1) - lower);
		port += lower;

		if (!is_local_udp_port_used(port)) {
			return port;
		}
	}
	return 0;
}




static bool is_remote_port_open_by_cwdaemon(const char * server, int port)
{
	struct timeval tv = { .tv_sec = 2 };

	char port_buf[16] = { 0 };
	snprintf(port_buf, sizeof (port_buf), "%d", port);
	int fd = cwdaemon_socket_connect(server, port_buf);
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof (tv));

	const char * requested_message_value = "e";
	const char * requested_reply_value   = "t";

	int a = cwdaemon_socket_send_request(fd, CWDAEMON_REQUEST_REPLY, requested_message_value);
	int b = cwdaemon_socket_send_request(fd, CWDAEMON_REQUEST_MESSAGE, requested_reply_value);

	/* Try receiving preconfigured reply. Receiving it means we
	   don't have to poll key for new key events because there
	   will be no more key events to receive (cwdaemon has
	   completed toggling tty pin). */
	char recv_buf[32] = { 0 };
	int r = recv(fd, recv_buf, sizeof (recv_buf), 0);
	close(fd);

	// TODO: we should compare recv_buf with requested_reply_value.

	//fprintf(stderr, "port %d, socket %d, send a %d, send b %d, rec %d\n", port, fd, a, b, r);

	return -1 != r;
}



