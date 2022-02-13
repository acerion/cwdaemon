/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2022 Kamil Ignacak <acerion@wp.pl>
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




#define _DEFAULT_SOURCE
#define _GNU_SOURCE /* strcasestr() */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>




#include <libcw.h>
#include <libcw2.h>




#include "../library/process.h"
#include "../library/socket.h"
#include "../library/cw_rec_utils.h"
#include "../library/key_source.h"
#include "../library/key_source_serial.h"
#include "../library/misc.h"



// LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/acerion/lib ~/sbin/cwdaemon -d ttyS0 -n -x p  -s 10 -T 1000 > /dev/null

static bool on_key_state_change(void * sink, bool key_is_down);
static int receive_from_key_source(int fd, cw_easy_receiver_t * easy_rec, char * buffer, size_t size);
static void send_to_cwdaemon(int fd, const char * text);

//static void * cwdaemon_stop_fn(void * child_arg);
//static int cwdaemon_request_message_and_receive(cwdaemon_process_t * child, const char * text, cw_easy_receiver_t * easy_rec);



/* global variables */
static cw_easy_receiver_t g_easy_rec = { 0 };
static const char * g_requested_reply = "sent";
static const char * g_expected_reply = "hsent\r\n"; /* Notice the initial 'h'. Notice terminating "\r\n". */



bool on_key_state_change(void * arg_easy_rec, bool key_is_down)
{
	cw_easy_receiver_t * easy_rec = (cw_easy_receiver_t *) arg_easy_rec;
	cw_easy_receiver_sk_event(easy_rec, key_is_down);
	// fprintf(stderr, "key is %s\n", key_is_down ? "down" : "up");

	return true;
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
static int receive_from_key_source(int fd, cw_easy_receiver_t * easy_rec, char * buffer, size_t size)
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
		if (-1 != r && 0 == strcmp(recv_buf, g_expected_reply)) {
			loop_iters = 0;
		}
	} while (loop_iters > 0);

	return 0;
}



int main(void)
{
	cwdaemon_process_t child = { 0 };
	int result = 0;

	cwdaemon_opts_t opts = {
		.tone           = "1000",
		.sound_system   = "p",
		.nofork         = "-n",
		.cwdevice       = "ttyS0",
	};
	snprintf(opts.wpm, sizeof (opts.wpm), "%d", 10);

	const char * path = "/home/acerion/sbin/cwdaemon";
	if (0 != cwdaemon_start(path, &opts, &child)) {
		fprintf(stderr, "[EE] Failed to start cwdaemon, exiting\n");
		exit(EXIT_FAILURE);
	}

	const char * cwdaemon_address = "127.0.0.1";
	char cwdaemon_port[16] = { 0 };
	snprintf(cwdaemon_port, sizeof (cwdaemon_port), "%d", child.l4_port);
	cw_easy_receiver_t * easy_rec = &g_easy_rec;
#if 0
	cw_enable_adaptive_receive();
#else
	cw_set_receive_speed(10);
#endif

	cw_generator_new(CW_AUDIO_NULL, NULL);
	cw_generator_start();

	cw_register_keying_callback(cw_easy_receiver_handle_libcw_keying_event, easy_rec);
	cw_easy_receiver_start(easy_rec);

	cw_key_source_t source = {
		.open_fn            = cw_key_source_serial_open,
		.close_fn           = cw_key_source_serial_close,
		.new_key_state_cb   = on_key_state_change,
		.new_key_state_sink = easy_rec,
	};
	cw_key_source_configure_polling(&source, 0, cw_key_source_serial_poll_once);
	if (!source.open_fn(&source)) {
		return -1;
	}
	cw_key_source_start(&source);

	cw_clear_receive_buffer();
	cw_easy_receiver_clear(easy_rec);

	gettimeofday(&easy_rec->main_timer, NULL);

	child.fd = cwdaemon_socket_connect(cwdaemon_address, cwdaemon_port);
	fprintf(stderr, "connected on %d\n", child.fd);

	cwdaemon_request_t request = {
		.id              = CWDAEMON_REQUEST_MESSAGE,
		.value           = "paris",
		.requested_reply = "sent",
	};
	result += cwdaemon_request_message_and_receive(&child, &request, easy_rec);

	/* This should stop the cwdaemon that runs in background. */
	cwdaemon_process_do_delayed_termination(&child, 2 * 1000);

	result += cwdaemon_process_wait_for_exit(&child);

	/* Cleanup. */
	cw_generator_stop();
	cw_key_source_stop(&source);
	source.close_fn(&source);
	/* cwdaemon is stopped, but let's still try to close socket on our
	   end. */
	cwdaemon_socket_disconnect(child.fd);

	return result;
}




