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

#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/log.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/sleep.h"




static int helpers_configure(morse_receiver_t * morse_receiver);
static int helpers_deconfigure(morse_receiver_t * morse_receiver);

static int libcw_receiver_configure(cw_easy_receiver_t * easy_receiver, int wpm);
static int libcw_receiver_deconfigure(__attribute__((unused)) cw_easy_receiver_t * easy_receiver);

static void * morse_receiver_thread_fn(void * receiver_arg);




#define PTT_EXPERIMENT 1

#if PTT_EXPERIMENT
typedef struct ptt_sink_t {
	int dummy;
} ptt_sink_t;




static ptt_sink_t g_ptt_sink = { 0 };




/// @brief Inform a ptt sink that a ptt pin has a new state (on or off)
///
/// @reviewed_on{2024.04.21}
///
/// @param arg_ptt_sink Receiver of information about state of ptt pin
/// @param ptt_is_on State of ptt pin
///
/// @return 0
static int ptt_sink_on_ptt_state_change(void * arg_ptt_sink, bool ptt_is_on)
{
	__attribute__((unused)) ptt_sink_t * ptt_sink = (ptt_sink_t *) arg_ptt_sink;
	test_log_debug("cwdevice observer: ptt sink: ptt is %s\n", ptt_is_on ? "on" : "off");

	return 0;
}
#endif /* #if PTT_EXPERIMENT */




int morse_receiver_configure(const morse_receiver_config_t * config, morse_receiver_t * receiver)
{
	pthread_attr_init(&receiver->thread.thread_attr);

	receiver->thread.status = thread_not_started;
	receiver->thread.name = "Morse receiver thread"; // TODO (acerion) 2024.04.21: set name of a thread using pthread API.
	receiver->thread.thread_fn = morse_receiver_thread_fn;
	receiver->thread.thread_fn_arg = receiver;

	receiver->config = *config;

	return 0;
}




void morse_receiver_deconfigure(morse_receiver_t * receiver)
{
	if (NULL == receiver) {
		return;
	}
	thread_dtor(&receiver->thread);

	return;
}




int morse_receiver_start(morse_receiver_t * receiver)
{
	// Helpers must be "fresh" for each run of receiver, therefore they are
	// configured in receiver's "start" function.
	if (0 != helpers_configure(receiver)) {
		test_log_err("Failed to configure helpers for Morse receiver %s\n", "");
		return -1;
	}

	if (0 != thread_start(&receiver->thread)) {
		test_log_err("Failed to start Morse receiver thread %s\n", "");
		return -1;
	}

	return 0;
}




int morse_receiver_wait_for_stop(morse_receiver_t * receiver)
{
	pthread_join(receiver->thread.thread_id, NULL);

	// Helpers aren't needed anymore, so deconfigure them. They will be
	// configured again during start of receiver, in receiver's "start"
	// function.
	helpers_deconfigure(receiver);
	return 0;
}




/// @brief Configure and start a libcw receiver that is used to do actual decoding of Morse code
///
/// @reviewed_on{2024.04.22}
///
/// @param easy_receiver Easy receiver to configure and start
/// @param wpm Expected speed of Morse code to be used for initialization of the receiver
///
/// @return 0
static int libcw_receiver_configure(cw_easy_receiver_t * easy_receiver, int wpm)
{
#if 0
	cw_enable_adaptive_receive();
#else
	cw_set_receive_speed(wpm);
#endif

	cw_generator_new(CW_AUDIO_NULL, NULL);
	cw_generator_start();

	cw_register_keying_callback(cw_easy_receiver_handle_libcw_keying_event_void, easy_receiver);
	cw_easy_receiver_start(easy_receiver);
	cw_clear_receive_buffer();
	cw_easy_receiver_clear(easy_receiver);

	return 0;
}




/// @brief Stop and deconfigure a libcw receiver that is used to do actual
/// decoding of Morse code
///
/// @reviewed_on{2024.04.21}
///
/// @param easy_receiver libcw receiver to stop and deconfigure
///
/// @return 0
static int libcw_receiver_deconfigure(__attribute__((unused)) cw_easy_receiver_t * easy_receiver)
{
	cw_generator_stop();
	return 0;
}




/// @brief Configure Morse receiver's helpers
///
/// In order to make a proper receive and decoding of Morse code on cwdevice,
/// the @p morse_receiver is using two helpers that are members of @c
/// morse_receiver_t: "observer of cwdevice" and "libcw cw receiver".
///
/// Call helpers_deconfigure() to clean-up the helpers configured in this function
///
/// @reviewed_on{2024.04.22}
///
/// @param morse_receiver Morse receiver for which to configure the helpers
static int helpers_configure(morse_receiver_t * morse_receiver)
{
	cwdevice_observer_t * cwdevice_observer = &morse_receiver->cwdevice_observer;
	cw_easy_receiver_t * libcw_receiver = &morse_receiver->libcw_receiver;
	memset(cwdevice_observer, 0, sizeof (cwdevice_observer_t));
	memset(libcw_receiver, 0, sizeof (cw_easy_receiver_t));

	// cwdevice observer is configured here in 3 steps. I believe that such
	// multi-step and more explicit setup gives better idea about
	// interactions between the observer and libcw receiver.

	/* Configure observer to observe tty cwdevice. */
	if (0 != cwdevice_observer_tty_setup(cwdevice_observer, &morse_receiver->config.observer_tty_pins_config)) {
		test_log_err("Morse receiver thread: failed to set up observer of cwdevice %s\n", "");
		return -1;
	}

	/* Changes of cwdevice's keying pin will be forwarded to libcw_receiver. */
	if (0 != cwdevice_observer_set_key_change_handler(cwdevice_observer, cw_easy_receiver_on_key_state_change, libcw_receiver)) {
		test_log_err("Morse receiver thread: failed to set up handler of key pin %s\n", "");
		return -1;
	}

#if PTT_EXPERIMENT
	/* Changes of cwdevice's ptt pin will be forwarded to g_ptt_sink. */
	if (0 != cwdevice_observer_set_ptt_change_handler(cwdevice_observer, ptt_sink_on_ptt_state_change, &g_ptt_sink)) {
		test_log_err("Morse receiver thread: failed to set up handler of ptt pin %s\n", "");
		return -1;
	}
#endif

	if (0 != cwdevice_observer_start_observing(cwdevice_observer)) {
		test_log_err("Morse receiver thread: failed to start up cwdevice observer %s\n", "");
		return -1;
	}

	const int wpm = morse_receiver->config.wpm == 0 ? CW_SPEED_INITIAL : morse_receiver->config.wpm;
	if (0 != libcw_receiver_configure(libcw_receiver, wpm)) {
		test_log_err("Morse receiver thread: failed to set up Morse receiver %s\n", "");
		return -1;
	}

	// Make sure that libcw receiver and cwdevice observer are in sync with
	// regards to *initial* state of keying pin. Both objects store some kind
	// of "previous state of key" information, and that information must be
	// the same in both objects.
	//
	// Observer learns the initial state of the pin only during a start, in
	// cwdevice_observer_start_observing().
	libcw_receiver->tracked_key_is_down = cwdevice_observer->previous_key_is_down;

	return 0;
}




/// @brief Deconfigure Morse receiver's helpers
///
/// Call this function on a Morse receiver for which a setup of helpers was
/// done with helpers_configure().
///
/// @reviewed_on{2024.04.21}
///
/// @param morse_receiver Morse receiver for which to deconfigure helpers
///
/// @return 0
static int helpers_deconfigure(morse_receiver_t * morse_receiver)
{
	libcw_receiver_deconfigure(&morse_receiver->libcw_receiver);
	cwdevice_observer_stop_observing(&morse_receiver->cwdevice_observer);

	return 0;
}




/// @brief Receive Morse code, once
///
/// Function monitors cwdevice and decodes Morse message keyed on the device.
/// After the keying stops and some predefined time elapses without further
/// keying, the thread function exits.
///
/// The predefined time is in range of seconds.
///
/// @reviewed_on{2024.04.21}
///
/// @param receiver_arg morse_receiver_t variable
static void * morse_receiver_thread_fn(void * receiver_arg)
{
	// libcw_receiver is notified about changes of 'keying' pin by
	// cwdevice_observer. Those changes are translated by libcw_receiver into
	// characters and spaces. Periodically poll libcw_receiver to see if it
	// recognized some character or a space.
	//
	// The characters and spaces are put into 'buffer[]'.

	morse_receiver_t * morse_receiver = (morse_receiver_t *) receiver_arg;
	thread_t * thread = &morse_receiver->thread;
	thread->status = thread_running;

	/* FIXME acerion 2024.09.11: the code that inserts received chars into
	   the buffer doesn't check if there is enough space left in the
	   buffer. */
	char buffer[MORSE_RECV_BUFFER_SIZE] = { 0 };
	int buffer_i = 0;

	// A time that the loop can spend idling (without receiving any char or
	// inter-word-space) before the loop decides that nothing more will be
	// received.
	//
	// This value depends on wpm: for lower speeds the time should be higher,
	// for higher speeds it can be lower. TODO acerion 2024.01.27: calculate
	// the time using duration of longest character/representation as input.
	//
	// For  4 WMP the safe value appears to be 9000 ms
	// For 10 WPM the save value appears to be 7000 ms
	const int total_wait_ms = 7 * 1000; // [milliseconds].

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
		if (cw_easy_receiver_poll_data(&morse_receiver->libcw_receiver, &erd)) {
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

	const bool something_received = last_character_receive_tstamp.tv_sec != 0 && last_character_receive_tstamp.tv_nsec != 0;
	// TODO (acerion) 2024.04.18: we could also look at index variable used
	// to index buffer[] to see if something was received.
	if (something_received) {
		events_insert_morse_receive_event(morse_receiver->events, buffer, &last_character_receive_tstamp);
	}

	test_log_info("Morse receiver thread: received string [%s], remaining wait time = %d\n", buffer, remaining_wait_ms);

	thread->status = thread_stopped_ok;
	return NULL;
}




