/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
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
   Tests of "<ESC>h" request feature.
*/




#define _DEFAULT_SOURCE

#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "src/lib/random.h"
#include "src/lib/sleep.h"
#include "tests/library/cwdevice_observer.h"
#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/misc.h"
#include "tests/library/process.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"




static bool on_key_state_change(void * arg_easy_rec, bool key_is_down);




/* [milliseconds]. Total time for receiving a message (either receiving a
   Morse code message, or receiving a reply from cwdaemon server). */
#define RECEIVE_TOTAL_WAIT_MS (15 * 1000)


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

	snprintf(observer->source_path, sizeof (observer->source_path), "%s", "/dev/" TEST_CWDEVICE_NAME);

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
	tty_pins_t observer_tty_pins;            /**< Which tty pins on cwdevice should be treated by cwdevice as keying or ptt pins. */

	const char * message;                    /**< Text to be sent to cwdaemon server by cwdaemon client in a request. */
	const char * requested_reply_value;      /**< What is being sent to cwdaemon server as expected value of reply (without leading 'h'). */
	char actual_reply_value[32];             /**< What has been sent back from cwdaemon (including leading 'h' and terminating "\r\n"). */
} test_case_t;




static test_case_t g_test_cases[] = {
	/* This is a SUCCESS case. This is a basic case where cwdaemon is
	   executed without -o options, so it uses default tty lines. cwdevice
	   observer is configured to look at the default line(s) for keying
	   events. */
	{ .description             = "success case, empty reply value",
	  .observer_tty_pins       = { .pin_keying = TIOCM_DTR, .pin_ptt = TIOCM_RTS },

	  .message                 = "paris",
	  .requested_reply_value   = "",
	},

	/* This is a SUCCESS case. This is an almost-basic case where
	   cwdaemon is executed with -o options but the options still tell
	   cwdaemon to use default tty lines. cwdevice observer is configured to
	   look at the default line(s) for keying events. */
	{ .description             = "success case, single-letter as a value of reply",
	  .observer_tty_pins       = { .pin_keying = TIOCM_DTR, .pin_ptt = TIOCM_RTS },

	  .message                 = "paris",
	  .requested_reply_value   = "r",
	},

	/* This is a FAIL case. cwdaemon is told to toggle a DTR while
	   keying, but a cwdevice observer (and thus a receiver) is told to look at
	   RTS for keying events. */
	{ .description             = "success case, a word as value of reply",
	  .observer_tty_pins       = { .pin_keying = TIOCM_DTR, .pin_ptt = TIOCM_RTS },

	  .message                 = "paris",
	  .requested_reply_value   = "reply",
	},

	/* This is a SUCCESS case. cwdaemon is told to toggle a RTS while
	   keying, and a cwdevice observer (and thus a receiver) is told to look
	   also at RTS for keying events. */
	{ .description             = "success case, a sentence as a value of reply",
	  .observer_tty_pins       = { .pin_keying = TIOCM_DTR, .pin_ptt = TIOCM_RTS },

	  .message                 = "paris",
	  .requested_reply_value   = "I am a reply to your 27th request.",
	},
};




static char * escape_string(const char * buffer, char * escaped, size_t size)
{
	size_t e_idx = 0;

	for (size_t i = 0; i < strlen(buffer); i++) {
		switch (buffer[i]) {
		case '\r':
			escaped[e_idx++] = '\'';
			escaped[e_idx++] = 'C';
			escaped[e_idx++] = 'R';
			escaped[e_idx++] = '\'';
			break;
		case '\n':
			escaped[e_idx++] = '\'';
			escaped[e_idx++] = 'L';
			escaped[e_idx++] = 'F';
			escaped[e_idx++] = '\'';
			break;
		default:
			escaped[e_idx++] = buffer[i];
			break;
		}
	}

	escaped[size - 1] = '\0';
	return escaped;
}




static void * morse_receiver_thread_fn(void * test_case_arg)
{
	test_case_t * test_case = test_case_arg;
	cwdevice_observer_t cwdevice_observer = { 0 };
	cw_easy_receiver_t morse_receiver = { 0 };


	/* Preparation of test helpers. */
	{
		/* Prepare observer of cwdevice. */
		if (0 != cwdevice_observer_setup(&cwdevice_observer, &morse_receiver)) {
			fprintf(stderr, "[EE] Failed to set up observer of cwdevice\n");
			return NULL;
		}
		cwdevice_observer.tty_pins_config = test_case->observer_tty_pins; /* Observer of cwdevice should look at pins according to this config. */
		if (0 != cwdevice_observer_start(&cwdevice_observer)) {
			fprintf(stderr, "[EE] Failed to start up cwdevice observer\n");
			return NULL;
		}
		/* Prepare receiver of Morse code. */
		if (0 != morse_receiver_setup(&morse_receiver, 10)) { /* FIXME acerion 2023.12.28: replace magic number with wpm. */
			fprintf(stderr, "[EE] Failed to set up Morse receiver\n");
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
	do {
		const int sleep_retv = millisleep_nonintr(loop_iter_sleep_ms);
		if (sleep_retv) {
			fprintf(stderr, "[EE] error in sleep while receiving Morse code\n");
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
			} else {
				; /* NOOP */
			}
		}

	} while (loop_iters-- > 0);

	fprintf(stderr, "[II] Morse receiver received string [%s]\n", buffer);
	strncpy(test_case->actual_reply_value, buffer, sizeof (test_case->actual_reply_value));
	test_case->actual_reply_value[sizeof (test_case->actual_reply_value) - 1] = '\0';

	/* Cleanup of test helpers. */
	test_helpers_morse_receiver_desetup(&morse_receiver);
	cwdevice_observer_dtor(&cwdevice_observer);


	return NULL;
}




static void * socket_receiver_thread_fn(void * client_arg)
{
	client_t * client = (client_t *) client_arg;

	const int loop_iter_sleep_ms = 10; /* [milliseconds] Sleep duration in one iteration of a loop. TODO acerion 2023.12.28: use a constant. */
	const int loop_total_wait_ms = RECEIVE_TOTAL_WAIT_MS;
	int loop_iters = loop_total_wait_ms / loop_iter_sleep_ms;


	do {
		const int sleep_retv = millisleep_nonintr(loop_iter_sleep_ms);
		if (sleep_retv) {
			fprintf(stderr, "[EE] error in sleep while waiting for data on socket\n");
		}


		/* Try receiving preconfigured reply. */
		const ssize_t r = recv(client->sock, client->reply_buffer, sizeof (client->reply_buffer), MSG_DONTWAIT);
		if (-1 != r) {
			char escaped[64] = { 0 };
			fprintf(stderr, "[II] Received reply [%s] from cwdaemon server. Ending listening on the socket.\n", escape_string(client->reply_buffer, escaped, sizeof (escaped)));
			break;
		}
	} while (loop_iters-- > 0);

	if (loop_iters <= 0) {
		fprintf(stderr, "[NN] Expected reply from cwdaemon was not received within given time\n");
	}

	return 0;
}




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
		.tone           = "700",
		.sound_system   = CW_AUDIO_SOUNDCARD,
		.nofork         = true,
		.cwdevice_name  = TEST_CWDEVICE_NAME,
		.wpm            = wpm,
	};

	const size_t n = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	for (size_t i = 0; i < n; i++) {
		test_case_t * test_case = &g_test_cases[i];
		fprintf(stderr, "\n[II] Starting test case %zd/%zd: %s\n", i + 1, n, test_case->description);

		bool failure = false;
		cwdaemon_process_t cwdaemon = { 0 };
		client_t client = { 0 };

		char message_buf[64] = { 0 }; /* Message to be sent to cwdaemon server. */

		char expected_reply[64] = { 0 };
		char escaped_expected[64] = { 0 };
		char escaped_actual[64] = { 0 };


		pthread_t socket_thread_id = { 0 };
		pthread_t morse_receiver_thread_id = { 0 };
		pthread_attr_t socket_thread_attr;
		pthread_attr_t morse_receiver_thread_attr;
		pthread_attr_init(&socket_thread_attr);
		pthread_attr_init(&morse_receiver_thread_attr);



		/* Prepare local test instance of cwdaemon server. */
		if (0 != cwdaemon_start_and_connect(&server_opts, &cwdaemon, &client)) {
			fprintf(stderr, "[EE] Failed to start cwdaemon server, terminating\n");
			failure = true;
			goto cleanup;
		}




		pthread_create(&socket_thread_id, &socket_thread_attr, socket_receiver_thread_fn, &client);
		pthread_create(&morse_receiver_thread_id, &morse_receiver_thread_attr, morse_receiver_thread_fn, test_case);



		/*
		  The actual testing is done here.

		  First we ask cwdaemon to remember a reply that should be sent back
		  to us after a message is played.

		  Then we send the message itself.

		  Then we wait for completion of job by:
		  - Morse receiver thread that decodes a Morse code on cwdevice,
		  - socket receiver that receives the remembered reply - this is the
            most important part of this test.
		*/

		/* Ask cwdaemon to send us this reply back after playing a message. */
		client_send_request(&client, CWDAEMON_REQUEST_REPLY, test_case->requested_reply_value);

		/* Send the message to be played. */
		snprintf(message_buf, sizeof (message_buf), "start %s", test_case->message);
		client_send_request(&client, CWDAEMON_REQUEST_MESSAGE, message_buf);


		pthread_join(socket_thread_id, NULL);
		pthread_join(morse_receiver_thread_id, NULL);


		/* Compare the expected reply with actual reply. Notice that cwdaemon
		   server always prefixes the actual reply with 'h', and always
		   appends "\r\n". */
		snprintf(expected_reply, sizeof (expected_reply), "h%s\r\n", test_case->requested_reply_value);
		if (0 != strcmp(expected_reply, client.reply_buffer)) {
			fprintf(stderr, "[EE] Unexpected reply from cwdaemon server: expected [%s], got [%s]\n",
			        escape_string(expected_reply, escaped_expected, sizeof (escaped_expected)),
			        escape_string(client.reply_buffer, escaped_actual, sizeof (escaped_actual)));
			failure = true;
		} else {
			fprintf(stderr, "[II] Correct reply from cwdaemon server: expected [%s], got [%s]\n",
			        escape_string(expected_reply, escaped_expected, sizeof (escaped_expected)),
			        escape_string(client.reply_buffer, escaped_actual, sizeof (escaped_actual)));
		}


	cleanup:

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
			fprintf(stderr, "[EE] Test case %zd/%zd failed, terminating\n", i + 1, n);
			exit(EXIT_FAILURE);
		} else {
			fprintf(stderr, "[II] Test case %zd/%zd succeeded\n\n", i + 1, n);
		}
	}




	exit(EXIT_SUCCESS);
}



