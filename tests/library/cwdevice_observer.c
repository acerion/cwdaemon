/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2022 - 2024 Kamil Ignacak <acerion@wp.pl>
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

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   @file Top-level code for observing state of pins on cwdaemon's cwdevice
*/




#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>




#include "cwdevice_observer.h"
#include "log.h"
#include "misc.h"
#include "tests/library/sleep.h"




/// Default interval for polling a cwdevice [microseconds].
///
/// TODO (acerion) 2024.04.16: describe where this value comes from. Maybe
/// it's not the best default.
#define KEY_SOURCE_DEFAULT_INTERVAL_US 100




static void * cwdevice_observer_poll_thread(void * arg_observer);




int cwdevice_observer_start_observing(cwdevice_observer_t * observer)
{
	if (observer->open_fn) {
		if (0 != observer->open_fn(observer)) {
			test_log_err("cwdevice observer: failed to open cwdevice '%s' when starting observation of cwdevice\n", observer->source_path);
			return -1;
		}
	}

	observer->do_polling = true;
	const int retv = pthread_create(&observer->thread_id, NULL, cwdevice_observer_poll_thread, observer);
	if (0 != retv) {
		test_log_err("cwdevice observer: failed to create an observer thread: %d\n", retv);
		observer->do_polling = false;
		if (observer->close_fn) {
			observer->close_fn(observer);
		}
		return -1;
	}

	observer->thread_created = true;
	return 0;
}




void cwdevice_observer_stop_observing(cwdevice_observer_t * observer)
{
	if (observer->thread_created) {
		observer->do_polling = false;
		pthread_cancel(observer->thread_id);
		observer->thread_created = false;
	}

	if (observer->close_fn) {
		observer->close_fn(observer);
	}
}




/// @brief Thread function polling state of cwdevice's pins
///
/// @reviewed_on{2024.04.16}
static void * cwdevice_observer_poll_thread(void * arg_observer)
{
	cwdevice_observer_t * observer = (cwdevice_observer_t *) arg_observer;
	while (observer->do_polling) {
		bool key_is_down = false;
		bool ptt_is_on = false;

		if (0 != observer->poll_once_fn(observer, &key_is_down, &ptt_is_on)) {
			test_log_err("cwdevice observer: failed to poll once %s\n", "");
			return NULL;
		}

		/* Recognize new state, save it and react to it. */
		if (key_is_down != observer->previous_key_is_down) {
			observer->previous_key_is_down = key_is_down;
			if (observer->new_key_state_cb) {
				/* We may forward state of key to libcw's Morse
				   receiver/decoder, and the receiver will try to decode
				   characters and spaces. */
				observer->new_key_state_cb(observer->new_key_state_cb_arg, key_is_down);
			}
		}
		if (ptt_is_on != observer->previous_ptt_is_on) {
			observer->previous_ptt_is_on = ptt_is_on;
			if (observer->new_ptt_state_cb) {
				observer->new_ptt_state_cb(observer->new_ptt_state_cb_arg, ptt_is_on);
			}
		}

		const int sleep_retv = test_microsleep_nonintr(observer->poll_interval_us);
		if (sleep_retv) {
			test_log_err("cwdevice observer: error in sleep in key poll %s\n", "");
		}
	}

	return NULL;
}




void cwdevice_observer_configure_polling(cwdevice_observer_t * observer, unsigned int interval_us, poll_once_fn_t poll_once_fn)
{
	if (0 == interval_us) {
		observer->poll_interval_us = KEY_SOURCE_DEFAULT_INTERVAL_US;
	} else {
		observer->poll_interval_us = interval_us;
	}

	observer->poll_once_fn = poll_once_fn;
}




int cwdevice_observer_set_key_change_handler(cwdevice_observer_t * observer, int (* cb)(void * obj, bool key_is_down), void * obj)
{
	observer->new_key_state_cb     = cb;
	observer->new_key_state_cb_arg = obj;

	return 0;
}




int cwdevice_observer_set_ptt_change_handler(cwdevice_observer_t * observer, int (* cb)(void * obj, bool ptt_is_on), void * obj)
{
	observer->new_ptt_state_cb     = cb;
	observer->new_ptt_state_cb_arg = obj;

	return 0;
}

