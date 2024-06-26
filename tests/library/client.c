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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   @file

   Code for cwdaemon client - an entity connecting to local or remote
   cwdaemon server over network socket.

   Most of the time the communication is from client to server, but once in a
   while the client can expect to receive some reply from the server (e.g. a
   reply to <ESC>h request).
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

#include "tests/library/sleep.h"




// Poll interval for client's receiver polling [milliseconds].
#define RECEIVE_THREAD_INTERVAL_MS     1000

// How long to wait for termination of receive thread [milliseconds].
#define RECEIVE_THREAD_STOP_WAIT_MS   (RECEIVE_THREAD_INTERVAL_MS + (RECEIVE_THREAD_INTERVAL_MS * 0.2))




static void * client_socket_receiver_thread_poll_fn(void * client_arg);




int client_send_esc_request(client_t * client, int code, const char * bytes, size_t n_bytes)
{
	/* This buffer will store opaque data, so we don't have to think whether there is space for terminating NUL. */
	test_request_t request = { 0 };

	size_t i = 0;
	switch (code) {
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
		request.bytes[i++] = ASCII_ESC;
		request.bytes[i++] = (char) code;
		/*
		  Some of the Escape requests don't require a value, but we always
		  copy all bytes of value to network message to test cwdaemon's
		  behaviour in unexpected situations.

		  Test code may pass to the function any sequence of bytes to test
		  server's response. The purpose of this function is to send the
		  bytes verbatim over socket to cwdaemon.

		  We use memcpy() because we don't care what caller has passed
		  through @p bytes. We just send @p n_bytes of data through socket.
		*/
		if (n_bytes + i > sizeof (request.bytes)) {
			test_log_err("cwdaemon client: size of data to send to cwdaemon server as Escape request is too large: %zu + %zu > %zu\n", n_bytes, i, sizeof (request.bytes));
			return -1;
		}
		memcpy(request.bytes + i, bytes, n_bytes);
		request.n_bytes = i + n_bytes;
		return client_send_request(client, &request);
	default:
		test_log_err("cwdaemon client: unsupported Escape request code 0x%02x / '%c' / %d\n", (unsigned char) code, (char) code, code);
		return -1;
	}
}




int client_send_request(client_t * client, const test_request_t * request)
{
	errno = 0;
	const ssize_t send_rc = send(client->sock, request->bytes, request->n_bytes, 0);
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
		test_log_err("cwdaemon client: can't disconnect a client that is NULL %s\n", "");
		return -1;
	}

	if (client->sock < 0) {
		test_log_warn("cwdaemon client: can't disconnect a client that has closed socket %s\n", "");
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

		const int timeout_ms = RECEIVE_THREAD_INTERVAL_MS;
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
				get_printable_string(client->received_data.bytes, client->received_data.n_bytes, printable, sizeof (printable));
				test_log_info("cwdaemon client: received %zu/[%s] from cwdaemon server\n",
				              client->received_data.n_bytes, printable);
				events_insert_reply_received_event(client->events, &client->received_data);
			}
		} else {
			test_log_err("cwdaemon client: poll() error %s\n", "");
		}
	}

	// TODO (acerion) 2024.06.02: return thread's status through return
	// value, capture it with pthread_join().
	thread->status = thread_stopped_ok;
	return NULL;
}




int client_socket_receive_enable(client_t * client)
{
	pthread_attr_init(&client->socket_receiver_thread.thread_attr);
	client->socket_receiver_thread.status = thread_not_started;
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
	if (NULL == client->socket_receiver_thread.thread_fn) {
		test_log_err("cwdaemon client: trying to stop 'socket receive' thread without thread function %s\n", "");
		return -1;
	}

	test_log_info("cwdaemon client: stopping %s\n", client->socket_receiver_thread.name);
	client->socket_receiver_thread.thread_loop_continue = false;
	test_millisleep_nonintr(RECEIVE_THREAD_STOP_WAIT_MS);
	pthread_join(client->socket_receiver_thread.thread_id, NULL);
	test_log_info("cwdaemon client: stopped %s, status = %u\n", client->socket_receiver_thread.name, client->socket_receiver_thread.status);

	if (client->socket_receiver_thread.status != thread_stopped_ok) {
		test_log_err("cwdaemon client: thread's status is not OK: %u\n", client->socket_receiver_thread.status);
		return -1;
	}

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

	if (client->sock >= 0) {
		test_log_err("cwdaemon client: detected non-closed socket during destruction of client %s\n", "");
	}

	return success ? 0 : -1;
}




bool socket_receive_bytes_is_correct(const test_reply_data_t * expected, const test_reply_data_t * received)
{
	/*
	  This function is more than just simple memcmp() or strcmp() because I
	  want to validate here that "expected" data is valid too.

	  If I ever forget to end the "expected" value with CR+LF, then I may
	  miss a situation when cwdaemon server also doesn't insert CR+LF at the
	  end of its reply.

	  Facts to validate about "expected":
	  1. It ends with CR+LF (and perhaps with NUL),
	  2. It holds no more bytes than cwdaemon's "reply" buffer can hold; TODO acerion 2024.03.03 - add this test here.

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
		/* Pass. If a test says "we don't expect any reply in this
		   test case", it may use empty "expected" string to indicate
		   this. */
		// TODO (acerion) 2024.04.14: perhaps this function should not be
		// called when we know that no reply is expected? Then we should
		// return failure if expected->n_bytes is less than 2.
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

