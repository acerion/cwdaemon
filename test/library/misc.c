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

#include <libcw.h>
#include <libcw2.h>

#include "process.h"
#include "socket.h"
#include "misc.h"
#include "cw_rec_utils.h"
#include "key_source.h"
#include "key_source_serial.h"




static int receive_from_key_source(int fd, cw_easy_receiver_t * easy_rec, char * buffer, size_t size, const char * expected_reply);
static bool on_key_state_change(void * arg_easy_rec, bool key_is_down);
static int test_helper_key_source_setup(cw_key_source_t * key_source, const cw_key_source_params_t * key_source_params);
static int test_helper_easy_receiver_setup(cw_easy_receiver_t * easy_rec, const helpers_opts_t * opts);




/*
  These are global in this file because it's easier for me that way, and I
  don't plan to have tests that will require multiple parallel instances of
  easy receiver or key source.
*/
static cw_easy_receiver_t g_easy_rec = { 0 };
static cw_key_source_t g_key_source = {
	.open_fn            = cw_key_source_serial_open,
	.close_fn           = cw_key_source_serial_close,
	.new_key_state_cb   = on_key_state_change,
	.new_key_state_sink = &g_easy_rec,
};




/**
   Configure and start some objects used during tests of cwdaemon
*/
int test_helpers_setup(const helpers_opts_t * opts, const cw_key_source_params_t * key_source_params)
{
	if (0 != test_helper_easy_receiver_setup(&g_easy_rec, opts)) {
		fprintf(stderr, "[EE] Failed to set up easy receiver\n");
		return -1;
	}
	if (0 != test_helper_key_source_setup(&g_key_source, key_source_params)) {
		fprintf(stderr, "[EE] Failed to set up key source\n");
		return -1;
	}
	return 0;
}




/**
   Configure and start a receiver used during tests of cwdaemon
*/
static int test_helper_easy_receiver_setup(cw_easy_receiver_t * easy_rec, const helpers_opts_t * opts)
{
#if 0
	cw_enable_adaptive_receive();
#else
	cw_set_receive_speed(opts->wpm);
#endif

	cw_generator_new(CW_AUDIO_NULL, NULL);
	cw_generator_start();

	cw_register_keying_callback(cw_easy_receiver_handle_libcw_keying_event, easy_rec);
	cw_easy_receiver_start(easy_rec);
	cw_clear_receive_buffer();
	cw_easy_receiver_clear(easy_rec);

	// TODO acerion 2022.02.18 this seems to be not needed because it's
	// already done in cw_easy_receiver_start().
	//gettimeofday(&easy_rec->main_timer, NULL);

	return 0;
}




/**
   Configure and start a key source used during tests of cwdaemon
*/
static int test_helper_key_source_setup(cw_key_source_t * key_source, const cw_key_source_params_t * key_source_params)
{
	cw_key_source_configure_polling(key_source, 0, cw_key_source_serial_poll_once);
	if (!key_source->open_fn(key_source)) {
		return -1;
	}
	key_source->param_keying = key_source_params->param_keying;
	key_source->param_ptt    = key_source_params->param_ptt;
	cw_key_source_start(key_source);
	return 0;
}




void test_helpers_cleanup(void)
{
	/* Cleanup. */
	cw_generator_stop();
	cw_key_source_stop(&g_key_source);
	g_key_source.close_fn(&g_key_source);

	return;
}




int cwdaemon_play_text_and_receive(cwdaemon_process_t * cwdaemon, const char * message_value, bool expected_failed_receive)
{
	cw_easy_receiver_t * easy_rec = &g_easy_rec;

	/* When comparing strings, remember that a receiver may have received
	   first characters incorrectly. receive_from_key_source() prefixes a
	   text request with some startup text that is allowed to be
	   mis-received, so that the main part of text request is received
	   correctly and can be recognized with strcasestr(). */

	/* This sends a text request to cwdaemon that works in initial state,
	   i.e. reset command was not sent yet, so cwdaemon should not be
	   broken yet. */

	const char * requested_reply_value = "reply";
	char expected_reply[64] = { 0 };
	/* Notice the initial 'h'. Notice terminating "\r\n". */
	snprintf(expected_reply, sizeof (expected_reply), "h%s\r\n", requested_reply_value);


	/* Ask cwdaemon to send us this reply back after playint text, so
	   that we don't wait in receive_from_key_source() for longer than
	   it's necessary to play and key requested text. */
	cwdaemon_socket_send_request(cwdaemon->fd, CWDAEMON_REQUEST_REPLY, requested_reply_value);

	char value[64] = { 0 };
	snprintf(value, sizeof (value), "start %s", message_value);
	cwdaemon_socket_send_request(cwdaemon->fd, CWDAEMON_REQUEST_MESSAGE, value);


	char receive_buffer[30] = { 0 };
	if (0 != receive_from_key_source(cwdaemon->fd, easy_rec, receive_buffer, sizeof (receive_buffer), expected_reply)) {
		fprintf(stderr, "[EE] Failed to receive from key source\n");
		return -1;
	}

	const char * needle = strcasestr(receive_buffer, message_value);
	if (expected_failed_receive) {
		if (NULL == needle) {
			fprintf(stderr, "[II] Received text '%s' doesn't match sent text '%s', and a failed receive was expected\n",
				receive_buffer, message_value);
			return 0;
		} else {
			fprintf(stderr, "[EE] Received text '%s' matches sent text '%s', but a failed receive was expected\n",
				receive_buffer, message_value);
			return -1;
		}
	} else {
		if (NULL == needle) {
			fprintf(stderr, "[EE] Received text '%s' doesn't match sent text '%s\n",
				receive_buffer, message_value);
			return -1;
		} else {
			fprintf(stderr, "[II] Received text '%s', matches sent text '%s'\n",
				receive_buffer, message_value);
			return 0;
		}
	}
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
   @param[in] expected_reply value that we expect to receive

   @return 0 if no errors occurred
   @return -1 otherwise
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
	const int lower = 1024;
	const int upper = 65535;

	const int n = 1000; /* We should be able to find some unused port in 1000 tries, right? */
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

	cwdaemon_socket_send_request(fd, CWDAEMON_REQUEST_REPLY, requested_message_value);
	cwdaemon_socket_send_request(fd, CWDAEMON_REQUEST_MESSAGE, requested_reply_value);

	/* Try receiving preconfigured reply. Receiving it means that there
	   is a process on the other side of socket that behaves like
	   cwdaemon. */
	char recv_buf[32] = { 0 };
	int r = recv(fd, recv_buf, sizeof (recv_buf), 0);
	close(fd);

	// TODO: we should compare recv_buf with requested_reply_value.

	//fprintf(stderr, "port %d, socket %d, rec %d\n", port, fd, r);

	return -1 != r;
}




/**
   @brief Inform an easy receiver that a key has a new state (up or down)

   A simple wrapper that seems to be convenient.
*/
static bool on_key_state_change(void * arg_easy_rec, bool key_is_down)
{
	cw_easy_receiver_t * easy_rec = (cw_easy_receiver_t *) arg_easy_rec;
	cw_easy_receiver_sk_event(easy_rec, key_is_down);
	// fprintf(stderr, "key is %s\n", key_is_down ? "down" : "up");

	return true;
}




