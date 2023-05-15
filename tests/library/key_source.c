/*
 * Copyright (C) 2022 - 2023 Kamil Ignacak <acerion@wp.pl>
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




#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>




#include "key_source.h"
#include "misc.h"




/* Default interval for polling a key source [microseconds]. */
#define KEY_SOURCE_DEFAULT_INTERVAL_US 100




static void * key_source_poll_thread(void * arg_key_source);




void cw_key_source_start(cw_key_source_t * source)
{
	source->do_polling = true;
	pthread_create(&source->thread_id, NULL, key_source_poll_thread, source);
}




void cw_key_source_stop(cw_key_source_t * source)
{
	source->do_polling = false;
	pthread_cancel(source->thread_id);
}




static void * key_source_poll_thread(void * arg_key_source)
{
	cw_key_source_t * source = (cw_key_source_t *) arg_key_source;
	while (source->do_polling) {
		bool key_is_down = false;
		bool ptt_is_on = false;

		if (!source->poll_once_fn(source, &key_is_down, &ptt_is_on)) {
			fprintf(stderr, "[EE] Failed to poll once\n");
			return NULL;
		}

		/* TODO: ptt state is not being passed anywhere and is not
		   taken into consideration when testing cwdaemon. Use the
		   value somewhere. */
		if (key_is_down == source->previous_key_is_down && ptt_is_on == source->previous_ptt_is_on) {
			/* Key state and ptt state not changed, do nothing. */
		} else {
			source->previous_key_is_down = key_is_down;
			source->previous_ptt_is_on = ptt_is_on;
			source->new_key_state_cb(source->new_key_state_sink, key_is_down);
		}

		/* TODO 2022.01.26: use libcw.h/cw_usleep_internal() */
		int s = usleep_nonintr(source->poll_interval_us);
		if (s) {
			fprintf(stderr, "[WW] usleep in key poll was interrupted\n");
		}
	}

	return NULL;
}




void cw_key_source_configure_polling(cw_key_source_t * source, int interval_us, poll_once_fn_t poll_once_fn)
{
	if (0 == interval_us) {
		source->poll_interval_us = KEY_SOURCE_DEFAULT_INTERVAL_US;
	} else {
		source->poll_interval_us = interval_us;
	}

	source->poll_once_fn = poll_once_fn;
}


