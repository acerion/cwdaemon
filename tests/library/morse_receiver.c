/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
 * Copyright (C) 2012 - 2023 Kamil Ignacak <acerion@wp.pl>
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




#include "config.h"

#include <stdio.h>
#include <string.h>

#include <libcw.h>

#include "../library/cwdevice_observer.h"
#include "../library/cwdevice_observer_serial.h"
#include "../library/events.h"
#include "../library/misc.h"
#include "../library/thread.h"
#include "events.h"
#include "morse_receiver.h"
#include "src/lib/sleep.h"




/* [milliseconds]. Total time for receiving a message (either receiving a
   Morse code message, or receiving a reply from cwdaemon server). */
#define RECEIVE_TOTAL_WAIT_MS (15 * 1000)




extern events_t g_events;




/**
   Configure and start a receiver used during tests of cwdaemon
*/
int morse_receiver_setup(cw_easy_receiver_t * easy_rec, int wpm)
{
#if 0
	cw_enable_adaptive_receive();
#else
	cw_set_receive_speed(wpm);
#endif

	cw_generator_new(CW_AUDIO_NULL, NULL);
	cw_generator_start();

	cw_register_keying_callback(cw_easy_receiver_handle_libcw_keying_event, easy_rec);
	cw_easy_receiver_start(easy_rec);
	cw_clear_receive_buffer();
	cw_easy_receiver_clear(easy_rec);

	// TODO (acerion) 2022.02.18 this seems to be not needed because it's
	// already done in cw_easy_receiver_start().
	//gettimeofday(&easy_rec->main_timer, NULL);

	return 0;
}




int morse_receiver_desetup(__attribute__((unused)) cw_easy_receiver_t * easy_rec)
{
	cw_generator_stop();
	return 0;
}




void * morse_receiver_thread_fn(void * thread_arg)
{
	thread_t * thread = (thread_t *) thread_arg;
	cwdevice_observer_t cwdevice_observer = { 0 };
	cw_easy_receiver_t morse_receiver = { 0 };

	thread->status = thread_running;

	/* Preparation of test helpers. */
	{
		/* Prepare observer of cwdevice. */
		if (0 != cwdevice_observer_tty_setup(&cwdevice_observer, &morse_receiver)) {
			fprintf(stderr, "[EE] Morse receiver thread: failed to set up observer of cwdevice\n");
			thread->status = thread_stopped_err;
			return NULL;
		}
		if (0 != cwdevice_observer_start(&cwdevice_observer)) {
			fprintf(stderr, "[EE] Morse receiver thread: failed to start up cwdevice observer\n");
			thread->status = thread_stopped_err;
			return NULL;
		}
		/* Prepare receiver of Morse code. */
		if (0 != morse_receiver_setup(&morse_receiver, 10)) { /* FIXME acerion 2023.12.28: replace magic number with wpm. */
			fprintf(stderr, "[EE] Morse receiver thread: failed to set up Morse receiver\n");
			thread->status = thread_stopped_err;
			return NULL;
		}
	}


	char buffer[32] = { 0 };
	int buffer_i = 0;

	const int loop_iter_sleep_ms = 10; /* [milliseconds] Sleep duration in one iteration of a loop. TODO acerion 2023.12.28: use a constant. */
	const int loop_total_wait_ms = RECEIVE_TOTAL_WAIT_MS;
	int loop_iters = loop_total_wait_ms / loop_iter_sleep_ms;


	/*
	  Receiving a Morse code. cwdevice observer is telling a Morse code
	  receiver how a 'keying' pin on tty device is changing state, and the
	  receiver is translating this into text.
	*/
	struct timespec spec = { 0 };
	do {
		const int sleep_retv = millisleep_nonintr(loop_iter_sleep_ms);
		if (sleep_retv) {
			fprintf(stderr, "[EE] Morse receiver thread: error in sleep while receiving Morse code\n");
		}

		cw_rec_data_t erd = { 0 };
		if (cw_easy_receiver_poll_data(&morse_receiver, &erd)) {
			if (erd.is_iws) {
				fprintf(stderr, " ");
				fflush(stderr);
				buffer[buffer_i++] = ' ';
			} else if (erd.character) {
				fprintf(stderr, "%c", erd.character);
				fflush(stderr);
				buffer[buffer_i++] = erd.character;
				clock_gettime(CLOCK_MONOTONIC, &spec);
			} else {
				; /* NOOP */
			}
		}

	} while (loop_iters-- > 0);

	if (spec.tv_sec != 0 && spec.tv_nsec != 0) {
		pthread_mutex_lock(&g_events.mutex);
		{
			event_t * event = &g_events.events[g_events.event_idx];
			event->event_type = event_type_morse_receive;
			event->tstamp = spec;

			event_morse_receive_t * morse = &event->u.morse_receive;
			const size_t n = sizeof (morse->string);
			strncpy(morse->string, buffer, n);
			morse->string[n - 1] = '\0';

			g_events.event_idx++;
		}
		pthread_mutex_unlock(&g_events.mutex);
	}

	fprintf(stderr, "[II] Morse receiver received string [%s]\n", buffer);

	/* Cleanup of test helpers. */
	morse_receiver_desetup(&morse_receiver);
	cwdevice_observer_dtor(&cwdevice_observer);


	thread->status = thread_stopped_ok;
	return NULL;
}




