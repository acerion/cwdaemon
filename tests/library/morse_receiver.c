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




#define _GNU_SOURCE /* strcasestr() */




#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcw.h>

#include "../library/cwdevice_observer.h"
#include "../library/cwdevice_observer_serial.h"
#include "../library/events.h"
#include "../library/log.h"
#include "../library/misc.h"
#include "../library/thread.h"
#include "events.h"
#include "morse_receiver.h"
#include "sleep.h"




/* [milliseconds]. Total time for receiving a message (either receiving a
   Morse code message, or receiving a reply from cwdaemon server). */
#define RECEIVE_TOTAL_WAIT_MS (30 * 1000)




extern events_t g_events;




static int cw_receiver_desetup(__attribute__((unused)) cw_easy_receiver_t * easy_rec);
static void * morse_receiver_thread_fn(void * receiver_arg);




morse_receiver_t * morse_receiver_ctor(const morse_receiver_config_t * config)
{
	morse_receiver_t * receiver = (morse_receiver_t *) calloc(1, sizeof (morse_receiver_t));

	thread_ctor(&receiver->thread);

	receiver->thread.thread_fn = morse_receiver_thread_fn;
	receiver->thread.thread_fn_arg = receiver;
	receiver->thread.name = "Morse receiver thread";

	receiver->config = *config;

	return receiver;
}




void morse_receiver_dtor(morse_receiver_t ** receiver)
{
	if (NULL == receiver) {
		return;
	}
	if (NULL == *receiver) {
		return;
	}
	thread_dtor(&(*receiver)->thread);

	free(*receiver);
	*receiver = NULL;

	return;
}




int morse_receiver_start(morse_receiver_t * receiver)
{
	thread_start(&receiver->thread);
	return 0;
}




int morse_receiver_wait(morse_receiver_t * receiver)
{
	thread_join(&receiver->thread);
	return 0;
}




/**
   Configure and start a cw receiver (the libcw object) that is used during
   tests of cwdaemon
*/
static int cw_receiver_setup(cw_easy_receiver_t * easy_rec, int wpm)
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




/**
   Stop and deconfigure a cw receiver (the libcw object) that is used during
   tests of cwdaemon.
*/
static int cw_receiver_desetup(__attribute__((unused)) cw_easy_receiver_t * easy_rec)
{
	cw_generator_stop();
	return 0;
}




static void * morse_receiver_thread_fn(void * receiver_arg)
{
	morse_receiver_t * receiver = (morse_receiver_t *) receiver_arg;

	thread_t * thread = &receiver->thread;
	cwdevice_observer_t cwdevice_observer = { 0 };
	cw_easy_receiver_t cw_receiver = { 0 };

	thread->status = thread_running;

	/* Preparation of test helpers. */
	{
		/* Prepare observer of cwdevice. */
		if (0 != cwdevice_observer_tty_setup(&cwdevice_observer, &cw_receiver, &receiver->config.observer_tty_pins_config)) {
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
		const int wpm = receiver->config.wpm == 0 ? CW_SPEED_INITIAL : receiver->config.wpm;
		if (0 != cw_receiver_setup(&cw_receiver, wpm)) {
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
	struct timespec last_character_receive_tstamp = { 0 };
	do {
		const int sleep_retv = test_millisleep_nonintr(loop_iter_sleep_ms);
		if (sleep_retv) {
			fprintf(stderr, "[EE] Morse receiver thread: error in sleep while receiving Morse code\n");
		}

		cw_rec_data_t erd = { 0 };
		if (cw_easy_receiver_poll_data(&cw_receiver, &erd)) {
			if (erd.is_iws) {
				fprintf(stderr, " ");
				fflush(stderr);
				buffer[buffer_i++] = ' ';
			} else if (erd.character) {
				fprintf(stderr, "%c", erd.character);
				fflush(stderr);
				buffer[buffer_i++] = erd.character;
				clock_gettime(CLOCK_MONOTONIC, &last_character_receive_tstamp);
			} else {
				; /* NOOP */
			}
		}

	} while (loop_iters-- > 0);

	if (last_character_receive_tstamp.tv_sec != 0 && last_character_receive_tstamp.tv_nsec != 0) {
		events_insert_morse_receive_event(&g_events, buffer, &last_character_receive_tstamp);
	}

	fprintf(stderr, "[II] Morse receiver received string [%s]\n", buffer);

	/* Cleanup of test helpers. */
	cw_receiver_desetup(&cw_receiver);
	cwdevice_observer_dtor(&cwdevice_observer);


	thread->status = thread_stopped_ok;
	return NULL;
}




bool morse_receive_text_is_correct(const char * received_text, const char * expected_message)
{
	/*
	  When comparing strings, remember that a cw receiver may have received
	  first characters incorrectly. Text of message passed to
	  client_send_request*() is often prefixed with some startup text that is
	  allowed to be mis-received, so that the main part of text request is
	  received correctly and can be recognized with strcasestr().
	*/
	const char * needle = strcasestr(received_text, expected_message);
	return NULL != needle;
}




