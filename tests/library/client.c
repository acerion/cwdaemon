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




int client_send_request_va(client_t * client, int request, const char * format, ...)
{
	char buffer[1024] = { 0 };
	va_list ap;

	va_start(ap, format);
	vsnprintf(buffer, sizeof (buffer), format, ap);
	va_end(ap);

	return client_send_request(client, request, buffer);
}




int client_send_request(client_t * client, int request, const char * value)
{
	char buf[800] = { 0 };

	int i = 0;
	switch (request) {
	case CWDAEMON_ESC_REQUEST_RESET:
		buf[i++] = 27;
		buf[i++] = '0';
		/* This request doesn't require a value, but we insert the value to
		   network message to test cwdaemon's behaviour in unexpected
		   situation. */
		snprintf(buf + i, sizeof (buf) - i, "%s", value);
		break;
	case CWDAEMON_REQUEST_MESSAGE:
		/* Notice that we don't put Escape character in buf.
		   Regular text message is not an escaped request. */
		sprintf(buf, "%s", value);
		break;
	case CWDAEMON_ESC_REQUEST_SPEED:
		buf[0] = 27;
		sprintf(buf + 1, "2");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_ESC_REQUEST_TONE:
		buf[0] = 27;
		sprintf(buf + 1, "3");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_ESC_REQUEST_ABORT:
		buf[i++] = 27;
		buf[i++] = '4';
		/* This request doesn't require a value, but we insert the value to
		   network message to test cwdaemon's behaviour in unexpected
		   situation. */
		snprintf(buf + i, sizeof (buf) - i, "%s", value);
		break;
	case CWDAEMON_ESC_REQUEST_EXIT:
		buf[i++] = 27;
		buf[i++] = '5';
		/* This request doesn't require a value, but we insert the value to
		   network message to test cwdaemon's behaviour in unexpected
		   situation. */
		snprintf(buf + i, sizeof (buf) - i, "%s", value);
		break;
	case CWDAEMON_ESC_REQUEST_WORD_MODE:
		buf[i++] = 27;
		buf[i++] = '6';
		/* This request doesn't require a value, but we insert the value to
		   network message to test cwdaemon's behaviour in unexpected
		   situation. */
		snprintf(buf + i, sizeof (buf) - i, "%s", value);
		break;
	case CWDAEMON_ESC_REQUEST_WEIGHTING:
		buf[0] = 27;
		sprintf(buf + 1, "7");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_ESC_REQUEST_CWDEVICE:
		buf[0] = 27;
		sprintf(buf + 1, "8");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_ESC_REQUEST_PTT_STATE:
		buf[0] = 27;
		sprintf(buf + 1, "a");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_ESC_REQUEST_TUNE:
		buf[0] = 27;
		sprintf(buf + 1, "c");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_ESC_REQUEST_TX_DELAY:
		buf[0] = 27;
		sprintf(buf + 1, "d");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_ESC_REQUEST_SOUND_SYSTEM:
		buf[0] = 27;
		sprintf(buf + 1, "f");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_ESC_REQUEST_VOLUME:
		buf[0] = 27;
		sprintf(buf + 1, "g");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_ESC_REQUEST_REPLY:
		buf[0] = 27;
		sprintf(buf + 1, "h");
		sprintf(buf + 2, "%s", value);
		break;
	default:
		buf[0] = '\0';
		break;
	}

	/*
	  Notice that this line doesn't check contents of buf, so the buf may be
	  completely empty or it may contain garbage.

	  TODO acerion 2024.02.14: we always send "sizeof (buf)" bytes. Sometimes
	  we would want to vary the size, especially in fuzzing tests.
	*/
	errno = 0;
	const ssize_t send_rc = send(client->sock, buf, sizeof (buf), 0);
	if (send_rc == -1) {
		test_log_err("cwdaemon client: failed to send request to server: %s.\n", strerror(errno));
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

		memset(client->reply_buffer, 0, sizeof (client->reply_buffer));

		struct pollfd descriptor = { .fd = client->sock, .events = POLLIN };
		const nfds_t n_descriptors = 1;

		const int timeout_ms = (1 * 1000);
		const int ready = poll(&descriptor, n_descriptors, timeout_ms);
		if (0 == ready) {
			/* Timeout. */
			// test_log_debug("cwdaemon client: receive poll() timeout\n");
		} else if (n_descriptors == (nfds_t) ready) {
			if (descriptor.revents != POLLIN) {
				test_log_err("cwdaemon client: Unexpected event on poll socket: %02x\n", descriptor.revents);
				break;
			}
			const ssize_t r = recv(descriptor.fd, client->reply_buffer, sizeof (client->reply_buffer), MSG_DONTWAIT);
			if (-1 != r) {
				char escaped[64] = { 0 };
				test_log_info("cwdaemon client: received [%s] from cwdaemon server\n", escape_string(client->reply_buffer, escaped, sizeof (escaped)));
				events_insert_socket_receive_event(client->events, client->reply_buffer);
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

