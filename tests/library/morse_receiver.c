/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
 * Copyright (C) 2012 - 2024 Kamil Ignacak <acerion@wp.pl>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/// @file
///
/// Wrapper around "easy receiver", using it's API to:
///    - inform the easy receiver that observer of cwdevice is seeing changes of "keying" pin,
///    - poll from the receiver the received characters and inter-word-spaces.
///
///
/// @todo
/// \parblock
/// TODO (acerion) 2024.04.16 Refactor interaction between Morse
/// receiver, cw_easy_receiver and cwdevice observer.
///
/// The thread function in this file tries to do too much. Right now the
/// process of receiving Morse code is the only sink for events happening on
/// cwdevice's pin(s), so putting cwdevice observer inside of this file was
/// an easy choice. But it's a bad choice in the long run.
///
/// In the future the state of cwdevice's ptt pins will have to be also
/// somehow handled, and by something else than Morse receiver (even if this
/// "something else" is just an events store).
///
/// The cwdevice will somehow have to be shared by Morse receiver and by PTT
/// sink, or maybe Morse receiver and PT sink will have to be owned by
/// cwdevice observer. Perhaps the main object used by tests should be not
/// "Morse receiver" (as it is now) but "cwdevice observer".
/// \endparblock




#include "config.h"

#include <ctype.h>
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




#define PTT_EXPERIMENT 1

#if PTT_EXPERIMENT
typedef struct ptt_sink_t {
	int dummy;
} ptt_sink_t;




static ptt_sink_t g_ptt_sink = { 0 };




/**
   @brief Inform a ptt sink that a ptt pin has a new state (on or off)

   A simple wrapper that seems to be convenient.
*/
static int ptt_sink_on_ptt_state_change(void * arg_ptt_sink, bool ptt_is_on)
{
	__attribute__((unused)) ptt_sink_t * ptt_sink = (ptt_sink_t *) arg_ptt_sink;
	test_log_debug("cwdevice observer: ptt sink: ptt is %s\n", ptt_is_on ? "on" : "off");

	return 0;
}
#endif /* #if PTT_EXPERIMENT */




static int cw_receiver_desetup(__attribute__((unused)) cw_easy_receiver_t * easy_rec);
static void * morse_receiver_thread_fn(void * receiver_arg);




int morse_receiver_ctor(const morse_receiver_config_t * config, morse_receiver_t * receiver)
{
	thread_ctor(&receiver->thread);

	receiver->thread.thread_fn = morse_receiver_thread_fn;
	receiver->thread.thread_fn_arg = receiver;
	receiver->thread.name = "Morse receiver thread";

	receiver->config = *config;

	return 0;
}




void morse_receiver_dtor(morse_receiver_t * receiver)
{
	if (NULL == receiver) {
		return;
	}
	thread_dtor(&receiver->thread);

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
	morse_receiver_t * morse_receiver = (morse_receiver_t *) receiver_arg;

	/*
	  cw_receiver is notified about changes of 'keying' pin by
	  cwdevice_observer. Those changes are translated by libcw's cw_receiver
	  into characters and spaces. Periodically poll cw_receiver to see if it
	  recognized some character or a space.

	  The characters and spaces are put into 'buffer[]'.
	*/

	thread_t * thread = &morse_receiver->thread;
	cwdevice_observer_t cwdevice_observer = { 0 };
	cw_easy_receiver_t cw_receiver = { 0 };

	thread->status = thread_running;

	/* Preparation of test helpers. */
	{
		// cwdevice observer is configured here in 3 steps. I believe that such
		// multi-step and more explicit setup gives better idea about interactions
		// of the observer with cw receiver.

		/* Configure observer to observe tty cwdevice. */
		if (0 != cwdevice_observer_tty_setup(&cwdevice_observer, &morse_receiver->config.observer_tty_pins_config)) {
			test_log_err("Morse receiver thread: failed to set up observer of cwdevice %s\n", "");
			thread->status = thread_stopped_err;
			return NULL;
		}

		/* Changes of cwdevice's keying pin will be forwarded to cw_receiver. */
		if (0 != cwdevice_observer_set_key_change_handler(&cwdevice_observer, cw_easy_receiver_on_key_state_change, &cw_receiver)) {
			test_log_err("Morse receiver thread: failed to set up handler of key pin %s\n", "");
			thread->status = thread_stopped_err;
			return NULL;
		}

#if PTT_EXPERIMENT
		/* Changes of cwdevice's ptt pin will be forwarded to g_ptt_sink. */
		if (0 != cwdevice_observer_set_ptt_change_handler(&cwdevice_observer, ptt_sink_on_ptt_state_change, &g_ptt_sink)) {
			test_log_err("Morse receiver thread: failed to set up handler of ptt pin %s\n", "");
			thread->status = thread_stopped_err;
			return NULL;
		}
#endif

		if (0 != cwdevice_observer_start_observing(&cwdevice_observer)) {
			test_log_err("Morse receiver thread: failed to start up cwdevice observer %s\n", "");
			thread->status = thread_stopped_err;
			return NULL;
		}
		/* Prepare receiver of Morse code. */
		const int wpm = morse_receiver->config.wpm == 0 ? CW_SPEED_INITIAL : morse_receiver->config.wpm;
		if (0 != cw_receiver_setup(&cw_receiver, wpm)) {
			test_log_err("Morse receiver thread: failed to set up Morse receiver %s\n", "");
			thread->status = thread_stopped_err;
			return NULL;
		}
	}


	/* FIXME acerion 2024.09.11: the code that inserts received chars into
	   the buffer doesn't check if there is enough space left in the
	   buffer. */
	char buffer[MORSE_RECV_BUFFER_SIZE] = { 0 };
	int buffer_i = 0;

	/*
	  [milliseconds]. A time that the loop can spend idling (without
	  receiving any char or inter-word-space) before the loop decides that
	  nothing more will be received.

	  This value depends on wpm: for lower speeds the time should be higher,
	  for higher speeds it can be lower. TODO acerion 2024.01.27: calculate
	  the time using duration of longest character/representation as input.

	  For  4 WMP the safe value appears to be 9000 ms
	  For 10 WPM the save value appears to be 7000 ms
	*/
	const int total_wait_ms = 7 * 1000;

	const int poll_interval_ms = 10; /* [milliseconds]. TODO acerion 2024.01.27: use a constant. */
	int remaining_wait_ms = total_wait_ms;
	struct timespec last_character_receive_tstamp = { 0 };

	do {
		const int sleep_retv = test_millisleep_nonintr(poll_interval_ms);
		if (sleep_retv) {
			test_log_err("Morse receiver thread: error in sleep while receiving Morse code %s\n", "");
		}
		remaining_wait_ms -= poll_interval_ms;

		cw_rec_data_t erd = { 0 };
		if (cw_easy_receiver_poll_data(&cw_receiver, &erd)) {
			if (erd.is_iws) {
				fprintf(stdout, " ");
				fflush(stdout);
				buffer[buffer_i++] = ' ';
				test_log_debug("Morse receiver thread: received char %3d: ' ', remaining wait dropped to %d\n", buffer_i, remaining_wait_ms); /* Log shows 1-based char counter. */
				remaining_wait_ms = total_wait_ms; /* Reset remaining time. */
			} else if (erd.character) {
				fprintf(stdout, "%c", erd.character);
				fflush(stdout);
				buffer[buffer_i++] = erd.character;
				test_log_debug("Morse receiver thread: received char %3d: '%c', remaining wait dropped to %d\n", buffer_i, erd.character, remaining_wait_ms); /* Log shows 1-based char counter. */
				remaining_wait_ms = total_wait_ms; /* Reset remaining time. */
				clock_gettime(CLOCK_MONOTONIC, &last_character_receive_tstamp);
			} else {
				; /* NOOP */
			}
		}
	} while (remaining_wait_ms > 0);

	if (last_character_receive_tstamp.tv_sec != 0 && last_character_receive_tstamp.tv_nsec != 0) {
		events_insert_morse_receive_event(morse_receiver->events, buffer, &last_character_receive_tstamp);
	}

	test_log_info("Morse receiver thread: received string [%s], remaining wait time = %d\n", buffer, remaining_wait_ms);

	/* Cleanup of test helpers. */
	cw_receiver_desetup(&cw_receiver);
	cwdevice_observer_stop_observing(&cwdevice_observer);


	thread->status = thread_stopped_ok;
	return NULL;
}




