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
   Code for cwdaemon client - an entity connecting to local or remote
   cwdaemon server over network socket.

   Most of the time the communication is from client to server, but once in a
   while the client can expect to receive some reply from the server (e.g.
   thanks to <ESC>h request).
*/




#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client.h"
#include "events.h"
#include "misc.h"
#include "socket.h"
#include "thread.h"

#include "src/lib/sleep.h"




/* [milliseconds]. Total time for receiving a message (either receiving a
   Morse code message, or receiving a reply from cwdaemon server). */
#define RECEIVE_TOTAL_WAIT_MS (15 * 1000)




extern events_t g_events;




int client_send_request(client_t * client, int request, const char * value)
{
	char buf[80] = { 0 };

	switch (request) {
	case CWDAEMON_REQUEST_RESET:
		buf[0] = 27;
		sprintf(buf + 1, "0");
		break;
	case CWDAEMON_REQUEST_MESSAGE:
		/* Notice that we don't put Escape character in buf.
		   Regular text message is not an escaped request. */
		sprintf(buf, "%s", value);
		break;
	case CWDAEMON_REQUEST_SPEED:
		buf[0] = 27;
		sprintf(buf + 1, "2");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_TONE:
		buf[0] = 27;
		sprintf(buf + 1, "3");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_ABORT:
		buf[0] = 27;
		sprintf(buf + 1, "4");
		break;
	case CWDAEMON_REQUEST_EXIT:
		buf[0] = 27;
		sprintf(buf + 1, "5");
		break;
	case CWDAEMON_REQUEST_WORDMODE:
		buf[0] = 27;
		sprintf(buf + 1, "6");
		break;
	case CWDAEMON_REQUEST_WEIGHT:
		buf[0] = 27;
		sprintf(buf + 1, "7");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_DEVICE:
		buf[0] = 27;
		sprintf(buf + 1, "8");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_PTT:
		buf[0] = 27;
		sprintf(buf + 1, "a");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_TUNE:
		buf[0] = 27;
		sprintf(buf + 1, "c");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_TOD:
		buf[0] = 27;
		sprintf(buf + 1, "d");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_SDEVICE:
		buf[0] = 27;
		sprintf(buf + 1, "f");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_VOLUME:
		buf[0] = 27;
		sprintf(buf + 1, "g");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_REPLY:
		buf[0] = 27;
		sprintf(buf + 1, "h");
		sprintf(buf + 2, "%s", value);
		break;
	default:
		buf[0] = '\0';
		break;
	}

	ssize_t send_rc = -1;
	errno = 0;
	if (buf[0] != '\0') {
		send_rc = send(client->sock, buf, sizeof (buf), 0);
	}

	if (send_rc == -1) {
		fprintf(stderr, "[ERROR] Failed to send request to server: %s.\n", strerror(errno));
		return -1;
	} else {
		return 0;
	}
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
		perror("Failed to close client's socket to cwdaemon server.");
		client->sock = -1;
		return -1;
	}
	client->sock = -1;
	return 0;
}




void * client_socket_receiver_thread_fn(void * thread_arg)
{
	thread_t * thread = (thread_t *) thread_arg;
	client_t * client = (client_t *) thread->thread_fn_arg;
	thread->status = thread_running;

	const int loop_iter_sleep_ms = 10; /* [milliseconds] Sleep duration in one iteration of a loop. TODO acerion 2023.12.28: use a constant. */
	const int loop_total_wait_ms = RECEIVE_TOTAL_WAIT_MS;
	int loop_iters = loop_total_wait_ms / loop_iter_sleep_ms;


	do {
		const int sleep_retv = millisleep_nonintr(loop_iter_sleep_ms);
		if (sleep_retv) {
			fprintf(stderr, "[EE] error in sleep while waiting for data on socket\n");
		}


		/* Try receiving preconfigured reply. */
		const ssize_t r = recv(client->sock, client->reply_buffer, sizeof (client->reply_buffer), MSG_DONTWAIT);
		if (-1 != r) {
			char escaped[64] = { 0 };
			fprintf(stderr, "[II] Received reply [%s] from cwdaemon server. Ending listening on the socket.\n", escape_string(client->reply_buffer, escaped, sizeof (escaped)));
			struct timespec spec = { 0 };
			clock_gettime(CLOCK_MONOTONIC, &spec);

			pthread_mutex_lock(&g_events.mutex);
			{

				event_t * event = &g_events.events[g_events.event_idx];
				event->event_type = event_type_client_socket_receive;
				event->tstamp = spec;

				event_client_socket_receive_t * socket = &event->u.socket_receive;
				const size_t n = sizeof (socket->string);
				strncpy(socket->string, client->reply_buffer, n);
				socket->string[n - 1] = '\0';

				g_events.event_idx++;
			}
			pthread_mutex_unlock(&g_events.mutex);

			break;
		}
	} while (loop_iters-- > 0);

	if (loop_iters <= 0) {
		fprintf(stderr, "[NN] Expected reply from cwdaemon was not received within given time\n");
	}

	thread->status = thread_stopped_ok;
	return NULL;
}




