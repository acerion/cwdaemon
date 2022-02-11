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



#include <libcw.h>
#include <libcw2.h>




#include "../library/cw_rec_utils.h"
#include "../library/socket.h"
#include "../library/misc.h"
#include "key_source.h"
#include "key_source_serial.h"



static bool on_key_state_change(void * sink, bool key_is_down);




/* global variables */
static cw_easy_receiver_t g_easy_rec = { 0 };




bool on_key_state_change(void * arg_easy_rec, bool key_is_down)
{
	cw_easy_receiver_t * easy_rec = (cw_easy_receiver_t *) arg_easy_rec;
	cw_easy_receiver_sk_event(easy_rec, key_is_down);
	// fprintf(stderr, "key is %s\n", key_is_down ? "down" : "up");

	return true;
}




int main(void)
{
	srand(time(NULL));

	cwdaemon_process_t child = { 0 };
	cwdaemon_opts_t opts = {
		.tone               = "1000",
		.sound_system       = "p",
		.nofork             = true,
		.cwdevice           = "ttyS0",
		.use_random_l4_port = true
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
	int result = 0;

	/* This sends a text request to cwdaemon that works in initial state,
	   i.e. reset command was not sent yet, so cwdaemon should not be
	   broken yet. */
	cwdaemon_request_t request1 = {
		.id              = CWDAEMON_REQUEST_MESSAGE,
		.value           = "paris",
		.requested_reply = "sent",
	};
	result += cwdaemon_request_message_and_receive(&child, &request1, easy_rec);

	/* This would break the cwdaemon before a fix to
	   https://github.com/acerion/cwdaemon/issues/6 was applied. */
	cwdaemon_socket_send_request(child.fd, CWDAEMON_REQUEST_RESET, "");

	/* This sends a text request to cwdaemon that works in "after reset"
	   state. A fixed cwdaemon should reset itself correctly. */
	cwdaemon_request_t request2 = {
		.id              = CWDAEMON_REQUEST_MESSAGE,
		.value           = "texas",
		.requested_reply = "sent",
	};
	result += cwdaemon_request_message_and_receive(&child, &request2, easy_rec);

	/* Cleanup. */
	cw_generator_stop();
	cw_key_source_stop(&source);
	source.close_fn(&source);

	/* This should stop the cwdaemon that runs in background. */
	cwdaemon_process_do_delayed_termination(&child, 100);
	cwdaemon_process_wait_for_exit(&child);

	/* cwdaemon is stopped, but let's still try to close socket on our
	   end. */
	cwdaemon_socket_disconnect(child.fd);

	if (result) {
		fprintf(stderr, "[EE] Test failed, result = %d\n", result);
	}

	return result;
}




