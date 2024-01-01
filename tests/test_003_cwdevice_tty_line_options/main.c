/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
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

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   Test for "-o" cwdevice options.
*/




#define _DEFAULT_SOURCE

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../library/cwdevice_observer.h"
#include "../library/cwdevice_observer_serial.h"
#include "../library/misc.h"
#include "../library/process.h"
#include "../library/socket.h"
#include "../library/test_env.h"
#include "src/lib/random.h"
#include "src/lib/sleep.h"




static bool on_key_state_change(void * arg_easy_rec, bool key_is_down);




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
static bool on_ptt_state_change(void * arg_ptt_sink, bool ptt_is_on)
{
	__attribute__((unused)) ptt_sink_t * ptt_sink = (ptt_sink_t *) arg_ptt_sink;
	fprintf(stderr, "[DEBUG] ptt sink: ptt is %s\n", ptt_is_on ? "on" : "off");

	return true;
}
#endif /* #if PTT_EXPERIMENT */




/**
   Configure and start a receiver used during tests of cwdaemon
*/
static int morse_receiver_setup(cw_easy_receiver_t * easy_rec, int wpm)
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




static int test_helpers_morse_receiver_desetup(__attribute__((unused)) cw_easy_receiver_t * easy_rec)
{
	cw_generator_stop();
	return 0;
}




static int cwdevice_observer_setup(cwdevice_observer_t * observer, cw_easy_receiver_t * morse_receiver)
{
	memset(observer, 0, sizeof (cwdevice_observer_t));

	observer->open_fn  = cwdevice_observer_serial_open;
	observer->close_fn = cwdevice_observer_serial_close;
	observer->new_key_state_cb   = on_key_state_change;
	observer->new_key_state_sink = morse_receiver;

	snprintf(observer->source_path, sizeof (observer->source_path), "%s", "/dev/" TEST_TTY_CWDEVICE_NAME);

#if PTT_EXPERIMENT
	observer->new_ptt_state_cb  = on_ptt_state_change;
	observer->new_ptt_state_arg = &g_ptt_sink;
#endif

	cwdevice_observer_configure_polling(observer, 0, cwdevice_observer_serial_poll_once);

	if (!observer->open_fn(observer)) {
		fprintf(stderr, "[EE] Failed to open cwdevice '%s' in setup of observer\n", observer->source_path);
		return -1;
	}

	return 0;
}




/**
   @brief Inform an easy receiver that a key has a new state (up or down)

   A simple wrapper that seems to be convenient.
*/
static bool on_key_state_change(void * arg_easy_rec, bool key_is_down)
{
	cw_easy_receiver_t * easy_rec = (cw_easy_receiver_t *) arg_easy_rec;
	cw_easy_receiver_sk_event(easy_rec, key_is_down);
	// fprintf(stderr, "key is %s\n", key_is_down ? "down" : "up");

	return true;
}




typedef struct test_case_t {
	const char * description;                /**< Tester-friendly description of test case. */
	tty_pins_t server_tty_pins;              /**< Configuration of tty pins on cwdevice used by cwdaemon server. */
	const char * string_to_play;             /**< Text to be sent to cwdaemon server by cwdaemon client in a request. */
	bool expected_failed_receive;            /**< Is a failure of Morse-receiving process expected in this testcase? */
	tty_pins_t observer_tty_pins;            /**< Which tty pins on cwdevice should be treated by cwdevice as keying or ptt pins. */
} test_case_t;




static test_case_t g_test_cases[] = {
	/* This is a SUCCESS case. This is a basic case where cwdaemon is
	   executed without -o options, so it uses default tty lines.

	   Configuration of pins for cwdevice observer is implicitly default. */
	{ .description             = "success case, standard setup without tty line options passed to cwdaemon",
	  .string_to_play          = "paris",
	},

	/* This is a SUCCESS case. This is an almost-basic case where
	   cwdaemon is executed with -o options but the options still tell
	   cwdaemon to use default tty lines.

	   Configuration of pins for cwdevice observer is implicitly default. */
	{ .description             = "success case, standard setup with explicitly setting default tty lines options passed to cwdaemon",
	  .server_tty_pins         = { .explicit = true, .pin_keying = TIOCM_DTR, .pin_ptt = TIOCM_RTS },
	  .string_to_play          = "paris",
	},

	/* This is a FAIL case. cwdaemon is told to toggle a DTR while
	   keying, but a cwdevice observer (and thus a Morse receiver) is told to look at
	   RTS for keying events. */
	{ .description             = "failure case, cwdaemon is keying DTR, cwdevice observer is monitoring RTS",
	  .server_tty_pins         = { .explicit = true, .pin_keying = TIOCM_DTR, .pin_ptt = TIOCM_RTS },
	  .string_to_play          = "paris",
	  .expected_failed_receive = true,
	  .observer_tty_pins       = { .explicit = true, .pin_keying = TIOCM_RTS, .pin_ptt = TIOCM_DTR }
	},

	/* This is a SUCCESS case. cwdaemon is told to toggle a RTS while
	   keying, and a cwdevice observer (and thus a receiver) is told to look
	   also at RTS for keying events. */
	{ .description             = "success case, cwdaemon is keying RTS, cwdevice observer is monitoring RTS",
	  .server_tty_pins         = { .explicit = true, .pin_keying = TIOCM_RTS, .pin_ptt = TIOCM_DTR },
	  .string_to_play          = "paris",
	  .observer_tty_pins       = { .explicit = true, .pin_keying = TIOCM_RTS, .pin_ptt = TIOCM_DTR },
	},
};




int main(void)
{
#if 0
	if (!test_env_is_usable(test_env_libcw_without_signals)) {
		fprintf(stderr, "[EE] Preconditions for test env are not met, exiting\n");
		exit(EXIT_FAILURE);
	}
#endif

	const uint32_t seed = cwdaemon_srandom(0);
	fprintf(stderr, "[DD] Random seed: 0x%08x (%u)\n", seed, seed);

	const int wpm = 10;
	cwdaemon_opts_t server_opts = {
		.tone           = 660,
		.sound_system   = CW_AUDIO_SOUNDCARD,
		.nofork         = true,
		.cwdevice_name  = TEST_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
	};

	const size_t n = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	for (size_t i = 0; i < n; i++) {
		const test_case_t * test_case = &g_test_cases[i];
		fprintf(stderr, "\n[II] Starting test case #%zd: %s\n", i, test_case->description);

		bool failure = false;
		cwdevice_observer_t cwdevice_observer = { 0 };
		cw_easy_receiver_t morse_receiver = { 0 };
		cwdaemon_process_t cwdaemon = { 0 };
		client_t client = { 0 };



		/* Prepare local test instance of cwdaemon server. */
		server_opts.tty_pins = test_case->server_tty_pins; /* Server should toggle cwdevice pins according to this config. */
		if (0 != cwdaemon_start_and_connect(&server_opts, &cwdaemon, &client)) {
			fprintf(stderr, "[EE] Failed to start cwdaemon server, terminating\n");
			failure = true;
			goto cleanup;
		}



		/* Prepare observer of cwdevice. */
		if (0 != cwdevice_observer_setup(&cwdevice_observer, &morse_receiver)) {
			fprintf(stderr, "[EE] Failed to set up observer of cwdevice\n");
			failure = true;
			goto cleanup;
		}
		cwdevice_observer.tty_pins_config = test_case->observer_tty_pins; /* Observer of cwdevice should look at pins according to this config. */
		if (0 != cwdevice_observer_start(&cwdevice_observer)) {
			fprintf(stderr, "[EE] Failed to start up cwdevice observer\n");
			failure = true;
			goto cleanup;
		}



		/* Prepare receiver of Morse code. */
		if (0 != morse_receiver_setup(&morse_receiver, server_opts.wpm)) {
			fprintf(stderr, "[EE] Failed to set up Morse receiver\n");
			failure = true;
			goto cleanup;
		}



		/*
		  The actual testing is done here.

		  cwdaemon server will be now playing given string and will be keying
		  a specific line on tty (test_case->cwdaemon_param_keying).

		  The cwdevice observer will be observing a tty line that it was told
		  to observe (test_case->key_source_param_keying) and will be
		  notifying a Morse-receiver about keying events.

		  The Morse-receiver should correctly receive the text that cwdaemon
		  was playing (unless 'expected_failed_receive' is set to true).
		*/
		if (0 != client_send_and_receive_2(&client, &morse_receiver, test_case->string_to_play, test_case->expected_failed_receive)) {
			fprintf(stderr, "[EE] Failed at test of test_case #%zd\n", i);
			failure = true;
			goto cleanup;
		}




	cleanup:
		test_helpers_morse_receiver_desetup(&morse_receiver);
		cwdevice_observer_dtor(&cwdevice_observer);

		/* Terminate local test instance of cwdaemon. */
		if (0 != local_server_stop(&cwdaemon, &client)) {
			/*
			  Stopping a server is not a main part of a test, but if a
			  server can't be closed then it means that the main part of the
			  code has left server in bad condition. The bad condition is an
			  indication of an error in tested functionality. Therefore set
			  failure to true.
			*/
			fprintf(stderr, "[ERROR] Failed to correctly stop local test instance of cwdaemon.\n");
			failure = true;
		}

		/* Close our socket to cwdaemon server. */
		client_disconnect(&client);

		if (failure) {
			fprintf(stderr, "[EE] Test case #%zd failed, terminating\n", i);
			exit(EXIT_FAILURE);
		} else {
			fprintf(stderr, "[II] Test case #%zd succeeded\n\n", i);
		}
	}




	exit(EXIT_SUCCESS);
}



