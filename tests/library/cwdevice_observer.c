/*
 * Copyright (C) 2022 - 2023 Kamil Ignacak <acerion@wp.pl>
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




/**
   @file Top-level code for observing cwdaemon's cwdevice
*/




#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>




#include "cwdevice_observer.h"
#include "misc.h"
#include "src/lib/sleep.h"




/* Default interval for polling a key source [microseconds]. */
#define KEY_SOURCE_DEFAULT_INTERVAL_US 100




static void * key_source_poll_thread(void * arg_key_source);




void cw_key_source_start(cwdevice_observer_t * source)
{
	source->do_polling = true;
	pthread_create(&source->thread_id, NULL, key_source_poll_thread, source);
}




void cw_key_source_stop(cwdevice_observer_t * source)
{
	source->do_polling = false;
	pthread_cancel(source->thread_id);
}




static void * key_source_poll_thread(void * arg_key_source)
{
	cwdevice_observer_t * source = (cwdevice_observer_t *) arg_key_source;
	while (source->do_polling) {
		bool key_is_down = false;
		bool ptt_is_on = false;

		if (!source->poll_once_fn(source, &key_is_down, &ptt_is_on)) {
			fprintf(stderr, "[EE] Failed to poll once\n");
			return NULL;
		}

		/* Recognize new state, save it and react to it. */
		if (key_is_down != source->previous_key_is_down) {
			source->previous_key_is_down = key_is_down;
			if (source->new_key_state_cb) {
				source->new_key_state_cb(source->new_key_state_sink, key_is_down);
			}
		}
		if (ptt_is_on != source->previous_ptt_is_on) {
			source->previous_ptt_is_on = ptt_is_on;
			if (source->new_ptt_state_cb) {
				source->new_ptt_state_cb(source->new_ptt_state_arg, ptt_is_on);
			}
		}

		const int sleep_retv = microsleep_nonintr(source->poll_interval_us);
		if (sleep_retv) {
			fprintf(stderr, "[ERROR] error in sleep in key poll\n");
		}
	}

	return NULL;
}




void cw_key_source_configure_polling(cwdevice_observer_t * source, unsigned int interval_us, poll_once_fn_t poll_once_fn)
{
	if (0 == interval_us) {
		source->poll_interval_us = KEY_SOURCE_DEFAULT_INTERVAL_US;
	} else {
		source->poll_interval_us = interval_us;
	}

	source->poll_once_fn = poll_once_fn;
}


