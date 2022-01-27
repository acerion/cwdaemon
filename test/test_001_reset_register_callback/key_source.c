/*
 * Copyright (C) 2022 Kamil Ignacak <acerion@wp.pl>
 *
 * Code for polling of serial port is based on "statserial - Serial Port
 * Status Utility" (GPL2+).
 * Copyright (C) 1994 Jeff Tranter (Jeff_Tranter@Mitel.COM)
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




#define _DEFAULT_SOURCE /* usleep() */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>




#include "key_source.h"




static void * key_source_poll_thread(void * arg_key_source);




void key_source_start(cw_key_source_t * source)
{
	source->do_polling = true;
	pthread_create(&source->thread_id, NULL, key_source_poll_thread, source);
}




void key_source_stop(cw_key_source_t * source)
{
	source->do_polling = false;
	pthread_cancel(source->thread_id);
}




static void * key_source_poll_thread(void * arg_key_source)
{
	cw_key_source_t * source = (cw_key_source_t *) arg_key_source;
	while (source->do_polling) {
		bool key_is_down = false;

		if (!source->poll_once_fn(source, &key_is_down)) {
			fprintf(stderr, "[EE] Failed to poll once\n");
			return NULL;
		}

		/* avoid unnecessary screen updates */
		if (key_is_down == source->previous_key_is_down) {
			usleep(source->poll_interval_us);
			continue;
		}
		source->previous_key_is_down = key_is_down;

		source->new_key_state_cb(source->new_key_state_sink, key_is_down);

		usleep(source->poll_interval_us);
	}

	return NULL;
}






