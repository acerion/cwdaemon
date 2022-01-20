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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>



#include <libcw.h>
#include <libcw2.h>

#include "cw_rec_utils.h"
#include "../library/socket.h"
#include "key_source.h"
#include "key_source_serial.h"



// LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/acerion/lib ~/sbin/cwdaemon -d ttyS0 -n -x p  -s 10 -T 1000 > /dev/null


static bool on_key_state_change(void * sink, unsigned int arg);
static int receive_from_key_source(cw_easy_receiver_t * easy_rec, char * buffer, size_t size);
static void send_to_cwdaemon(int fd, const char * text);




/* global variables */
static cw_easy_receiver_t g_easy_rec = { 0 };




bool on_key_state_change(void * arg_easy_rec, unsigned int arg)
{
	cw_easy_receiver_t * easy_rec = (cw_easy_receiver_t *) arg_easy_rec;
	const bool is_down = !!(arg & TIOCM_DTR);
	cw_easy_receiver_sk_event(easy_rec, is_down);
	// fprintf(stderr, "DTR = %d\n", is_down);

	return true;
}




static void send_to_cwdaemon(int fd, const char * text)
{
	char value[30] = { 0 };
	snprintf(value, sizeof (value), "start %s", text);
	cwdaemon_send_request(fd, CWDAEMON_REQUEST_MESSAGE, value);
}




/**
   Our key source is DTR pin on serial line.

   The pin is changed by cwdaemon.

   Changes of the pin are polled by key source. Key source calls
   `new_key_state_cb` callback every time it detects change of the pin, i.e.
   change of key state.

   @p easy_rec is notified on each change of the key state.

   Here we poll @p easy_rec receiver to see what it received.
*/
static int receive_from_key_source(cw_easy_receiver_t * easy_rec, char * buffer, size_t size)
{
	memset(buffer, 0, size);
	int buffer_i = 0;

	int result = 0;
	for (int i = 0; i < 2000; i++) {
		usleep(10 * 1000); /* 10 milliseconds. */

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
	}
	return result;
}




int main(void)
{
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
		.poll_interval_us   = 100,
		.poll_once          = key_source_poll_once,
		.new_key_state_cb   = on_key_state_change,
		.new_key_state_sink = easy_rec,
	};
	if (!key_source_open(&source)) {
		return -1;
	}
	key_source_start(&source);

	cw_clear_receive_buffer();
	cw_easy_receiver_clear(easy_rec);

	gettimeofday(&easy_rec->main_timer, NULL);

	int fd = cwdaemon_connect("127.0.0.1", "6789");
	int result = 0;
	char receive_buffer[30] = { 0 };

	const char * text1 = "paris";
	send_to_cwdaemon(fd, text1);
	result += receive_from_key_source(easy_rec, receive_buffer, sizeof (receive_buffer));
	if (!strcasestr(receive_buffer, text1)) {
		result--;
	}

	cwdaemon_send_request(fd, CWDAEMON_REQUEST_RESET, "");

	const char * text2 = "texas";
	send_to_cwdaemon(fd, text2);
	result += receive_from_key_source(easy_rec, receive_buffer, sizeof (receive_buffer));
	if (!strcasestr(receive_buffer, text2)) {
		result--;
	}

	cw_generator_stop();
	key_source_stop(&source);
	key_source_close(&source);
	cwdaemon_disconnect(fd);

	return result;
}




