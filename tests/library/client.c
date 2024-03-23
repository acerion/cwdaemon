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
   Code for cwdaemon client - an entity connecting to local or remote
   cwdaemon server over network socket.

   Most of the time the communication is from client to server, but once in a
   while the client can expect to receive some reply from the server (e.g.
   thanks to <ESC>h request).
*/




#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client.h"
#include "events.h"
#include "log.h"
#include "misc.h"
#include "socket.h"
#include "string_utils.h"
#include "thread.h"

#include "sleep.h"




static void * client_socket_receiver_thread_poll_fn(void * client_arg);




int client_send_esc_request_va(client_t * client, int request, const char * format, ...)
{
	char buffer[CLIENT_SEND_BUFFER_SIZE] = { 0 };
	va_list ap;

	va_start(ap, format);
	vsnprintf(buffer, sizeof (buffer), format, ap);
	va_end(ap);

	return client_send_esc_request(client, request, buffer, sizeof (buffer)); /* TODO acerion 2024.02.28: last arg should be a value returned by vsnmprintf(). */
}




int client_send_esc_request(client_t * client, int request, const char * bytes, size_t n_bytes)
{
	/* This buffer will store opaque data, so we don't have to think whether there is space for terminating NUL. */
	char buf[CLIENT_SEND_BUFFER_SIZE] = { 0 };

	size_t n_bytes_to_send = 0; /* Total count of bytes to send over network socket. */
	int i = 0;
	switch (request) {
	case CWDAEMON_ESC_REQUEST_RESET:
	case CWDAEMON_ESC_REQUEST_SPEED:
	case CWDAEMON_ESC_REQUEST_TONE:
	case CWDAEMON_ESC_REQUEST_ABORT:
	case CWDAEMON_ESC_REQUEST_EXIT:
	case CWDAEMON_ESC_REQUEST_WORD_MODE:
	case CWDAEMON_ESC_REQUEST_WEIGHTING:
	case CWDAEMON_ESC_REQUEST_CWDEVICE:
	case CWDAEMON_ESC_REQUEST_PORT: /* Include it here even though it's not supported by cwdaemon anymore. */
	case CWDAEMON_ESC_REQUEST_PTT_STATE:
	case CWDAEMON_ESC_REQUEST_SSB_WAY:
	case CWDAEMON_ESC_REQUEST_TUNE:
	case CWDAEMON_ESC_REQUEST_TX_DELAY:
	case CWDAEMON_ESC_REQUEST_BAND_SWITCH:
	case CWDAEMON_ESC_REQUEST_SOUND_SYSTEM:
	case CWDAEMON_ESC_REQUEST_VOLUME:
	case CWDAEMON_ESC_REQUEST_REPLY:
		buf[i++] = ASCII_ESC;
		buf[i++] = (char) request;
		/*
		  Some of the escaped requests don't require a value, but we always
		  copy all bytes of value to network message to test cwdaemon's
		  behaviour in unexpected situations.

		  Test code may pass to the function any sequence of bytes to test
		  server's response. The purpose of this function is to send the
		  bytes verbatim over socket to cwdaemon.

		  We use memcpy() because we don't care what caller has passed
		  through @p bytes. We just send @p n_bytes of data through socket.
		*/
		if (n_bytes + i > sizeof (buf)) {
			test_log_err("cwdaemon client: size of data to send to cwdaemon server as escaped request is too large: %zu + %d > %zu\n", n_bytes, i, sizeof (buf));
			return -1;
		}
		memcpy(buf + i, bytes, n_bytes);
		n_bytes_to_send = n_bytes + i;
		return client_send_message(client, buf, n_bytes_to_send);
	default:
		test_log_err("cwdaemon client: unsupported escaped request 0x%02x / '%c' / %d\n", (unsigned char) request, (char) request, request);
		return -1;
	}
}




int client_send_message(client_t * client, const char * bytes, size_t n_bytes)
{
	errno = 0;
	const ssize_t send_rc = send(client->sock, bytes, n_bytes, 0);
	if (send_rc == -1) { /* TODO acerion 2024.02.28: shouldn't we compare the value with n_bytes? */
		test_log_err("cwdaemon client: failed to send data to server: %s\n", strerror(errno));
		return -1;
	}

	test_log_info("cwdaemon client: sent %ld bytes\n", send_rc);
	return 0;
}




int client_disconnect(client_t * client)
{
	if (NULL == client) {
		return -1;
	}

	if (client->sock < 0) {
		return 0;
	}
	if (close(client->sock) == -1) {
		test_log_err("cwdaemon client: failed to close client's socket to cwdaemon server: %s\n", strerror(errno));
		client->sock = -1;
		return -1;
	}
	client->sock = -1;
	return 0;
}




static void * client_socket_receiver_thread_poll_fn(void * client_arg)
{
	client_t * client = (client_t *) client_arg;
	thread_t * thread = &client->socket_receiver_thread;;

	thread->status = thread_running;

	while (thread->thread_loop_continue) {

		memset(&client->received_data, 0, sizeof (client->received_data));

		struct pollfd descriptor = { .fd = client->sock, .events = POLLIN };
		const nfds_t n_descriptors = 1;

		const int timeout_ms = (1 * 1000);
		const int ready = poll(&descriptor, n_descriptors, timeout_ms);
		if (0 == ready) {
			/* Timeout. */
			// test_log_debug("cwdaemon client: receive poll() timeout\n");
		} else if (n_descriptors == (nfds_t) ready) {
			if (descriptor.revents != POLLIN) {
				test_log_err("cwdaemon client: Unexpected event on poll socket: %02x\n", (unsigned int) descriptor.revents);
				break;
			}
			const ssize_t r = recv(descriptor.fd, client->received_data.bytes, sizeof (client->received_data.bytes), MSG_DONTWAIT);
			client->received_data.n_bytes = (size_t) -1;
			if (-1 != r) {
				client->received_data.n_bytes = (size_t) r;
				char printable[PRINTABLE_BUFFER_SIZE(sizeof (client->received_data.bytes))] = { 0 };
				get_printable_string(client->received_data.bytes, printable, sizeof (printable));
				test_log_info("cwdaemon client: received %zu/[%s] from cwdaemon server\n",
				              client->received_data.n_bytes, printable);
				events_insert_socket_receive_event(client->events, &client->received_data);
			}
		} else {
			test_log_err("cwdaemon client: poll() error %s\n", "");
		}
	}

	thread->status = thread_stopped_ok;
	return NULL;
}




int client_socket_receive_enable(client_t * client)
{
	thread_ctor(&client->socket_receiver_thread);
	client->socket_receiver_thread.name = "socket receiver thread";
	client->socket_receiver_thread.thread_fn = client_socket_receiver_thread_poll_fn;
	client->socket_receiver_thread.thread_fn_arg = client;

	return 0;
}




int client_socket_receive_start(client_t * client)
{
	if (NULL == client->socket_receiver_thread.thread_fn) {
		test_log_err("cwdaemon client: trying to start 'socket receive' thread without thread function %s\n", "");
		return -1;
	}
	client->socket_receiver_thread.thread_loop_continue = true;
	if (0 != thread_start(&client->socket_receiver_thread)) {
		test_log_err("cwdaemon client: failed to start socket receiver thread %s\n", "");
		return -1;
	}

	return 0;
}




int client_socket_receive_stop(client_t * client)
{
	test_log_info("cwdaemon client: stopping %s\n", client->socket_receiver_thread.name);
	client->socket_receiver_thread.thread_loop_continue = false;
	sleep(1);
	thread_join(&client->socket_receiver_thread);
	test_log_info("cwdaemon client: stopped %s\n", client->socket_receiver_thread.name);

	return 0;
}




int client_connect_to_server(client_t * client, const char * server_ip_address, in_port_t server_in_port)
{
	client->sock = open_socket_to_server(server_ip_address, server_in_port);
	if (client->sock < 0) {
		test_log_err("cwdaemon client: failed to connect to cwdaemon server socket at [%s:%u]\n", server_ip_address, server_in_port);
		return -1;
	}

	return 0;
}




int client_dtor(client_t * client)
{
	bool success = true;

	if (0 != thread_dtor(&client->socket_receiver_thread)) {
		test_log_err("cwdaemon client: failed to clean up '%s' thread\n", client->socket_receiver_thread.name);
		success = false;
	}

	return success ? 0 : -1;
}




bool socket_receive_bytes_is_correct(const socket_receive_data_t * expected, const socket_receive_data_t * received)
{
	/*
	  This function is more than just simple memcmp() or strcmp() because I
	  want to validate here that "expected" data is valid too.

	  If I ever forget to end the "expected" value with CR+LF, then I may
	  miss a situation when cwdaemon server also doesn't insert CR+LF at the
	  end of its reply.

	  Facts to validate about "expected":
	  1. It ends with CR+LF (and perhaps with NUL),
	  2. It holds no more bytes than cwdaemon's "reply" buffer; TODO acerion 2024.03.03 - add this test here.

	  Therefore one of the tests done here is looking for CR+LF in
	  "expected".
	*/

	if (expected->n_bytes > 0) {
		const size_t n = expected->n_bytes;
		if (expected->bytes[n - 2] != '\r' || expected->bytes[n - 1] != '\n') {
			test_log_err("Test: 'expected' data doesn't include terminating CR+LF. Fix your testcase data. %s\n", "");
			return false;
		}
	} else {
		/* Pass. If a test says "we don't expect any socket reply in this
		   test case", it may use empty "expected" string to indicate
		   this. */
	}

	if (received->n_bytes != expected->n_bytes) {
		test_log_err("Test: count of bytes in received and expected data doesn't match: %zu != %zu\n", received->n_bytes, expected->n_bytes);
		return false;
	}

	if (0 != memcmp(expected->bytes, received->bytes, expected->n_bytes)) {
		test_log_err("Test: contents of bytes in received and expected data doesn't match %s\n", "");
		return false;
	}

	return true;
}

