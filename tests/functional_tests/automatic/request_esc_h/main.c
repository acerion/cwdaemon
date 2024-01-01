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
   Tests of "<ESC>h request" feature.
*/




#define _DEFAULT_SOURCE
#define _GNU_SOURCE /* strcasestr() */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "src/lib/random.h"
#include "src/lib/sleep.h"
#include "tests/library/cwdevice_observer.h"
#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/events.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/process.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"
#include "tests/library/thread.h"
#include "tests/library/time_utils.h"




static bool on_key_state_change(void * arg_easy_rec, bool key_is_down);



events_t g_events = { .mutex = PTHREAD_MUTEX_INITIALIZER };




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
	const char * message;                    /**< Text to be sent to cwdaemon server by cwdaemon client in a request. */
	const char * requested_reply_value;      /**< What is being sent to cwdaemon server as expected value of reply (without leading 'h'). */
} test_case_t;




static test_case_t g_test_cases[] = {
	/* This is a SUCCESS case. We request cwdaemon server to send us empty
	   string in reply. */
	{ .description             = "success case, empty reply value",
	  .message                 = "paris",
	  .requested_reply_value   = "",
	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   single-letter string in reply. */
	{ .description             = "success case, single-letter as a value of reply",
	  .message                 = "paris",
	  .requested_reply_value   = "r",
	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   single-word string in reply. */
	{ .description             = "success case, a word as value of reply",
	  .message                 = "paris",
	  .requested_reply_value   = "reply",
	},

	/* This is a SUCCESS case. We request cwdaemon server to send us
	   full-sentence string in reply. */
	{ .description             = "success case, a sentence as a value of reply",
	  .message                 = "paris",
	  .requested_reply_value   = "I am a reply to your 27th request.",
	},
};




static void * morse_receiver_thread_fn(void * thread_arg)
{
	thread_t * thread = (thread_t *) thread_arg;
	cwdevice_observer_t cwdevice_observer = { 0 };
	cw_easy_receiver_t morse_receiver = { 0 };

	thread->status = thread_running;

	/* Preparation of test helpers. */
	{
		/* Prepare observer of cwdevice. */
		if (0 != cwdevice_observer_setup(&cwdevice_observer, &morse_receiver)) {
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
	test_helpers_morse_receiver_desetup(&morse_receiver);
	cwdevice_observer_dtor(&cwdevice_observer);


	thread->status = thread_stopped_ok;
	return NULL;
}




static void * socket_receiver_thread_fn(void * thread_arg)
{
	thread_t * thread = (thread_t *) thread_arg;
	client_t * client = (client_t *) thread->thread_fn_arg;
	thread->status = thread_running;

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
			struct timespec spec = { 0 };
			clock_gettime(CLOCK_MONOTONIC, &spec);

			pthread_mutex_lock(&g_events.mutex);
			{

				event_t * event = &g_events.events[g_events.event_idx];
				event->event_type = event_type_client_socket_receive;
				event->tstamp = spec;

				event_client_socket_receive_t * socket = &event->u.socket_receive;
				const size_t n = sizeof (socket->string);
				strncpy(socket->string, client->reply_buffer, n);
				socket->string[n - 1] = '\0';

				g_events.event_idx++;
			}
			pthread_mutex_unlock(&g_events.mutex);

			break;
		}
	} while (loop_iters-- > 0);

	if (loop_iters <= 0) {
		fprintf(stderr, "[NN] Expected reply from cwdaemon was not received within given time\n");
	}

	thread->status = thread_stopped_ok;
	return NULL;
}




/**
   Look at contents of @p events and check if order and types of events are
   as expected.

   @return 0 if events are in proper order and of proper type
   @return -1 otherwise
*/
static int events_evaluate(events_t * events, const test_case_t * test_case)
{
	const event_t * event_0 = &events->events[0];
	const event_t * event_1 = &events->events[1];
	const event_t * morse_event = NULL;
	const event_t * socket_event = NULL;




	/* Expectation 1: there should be only two events. */
	{
		if (2 != events->event_idx) {
			test_log_err("Unexpected count of events: %d\n", events->event_idx);
			return -1;
		}
	}




	/* Expectation 2: first event should be Morse receive, second event should be reply on socket. */
	{
		if (event_0->event_type == event_type_morse_receive
		    && event_1->event_type != event_type_client_socket_receive) {

			/*
			  This would be the correct order of events, but currently
			  (cwdaemon 0.11.0, 0.12.0) this is not the case: the order of
			  events is reverse. Right now I'm not willing to fix it yet.

			  TODO acerion 2023.12.30: fix the order of the two events in
			  cwdaemon. At the very least decrease the time difference
			  between the events from current ~300ms to few ms.
			*/
			morse_event = event_0;
			socket_event = event_1;
			; /* Pass. */
		} else {
			if (event_0->event_type == event_type_client_socket_receive
			    && event_1->event_type == event_type_morse_receive) {

				// This is the current incorrect behaviour that is accepted
				// for now.
				test_log_warn("Incorrect (but currently expected) order of events: %d -> %d\n",
				              event_0->event_type, event_1->event_type);
				socket_event = event_0;
				morse_event = event_1;
				; /* Pass. */
			} else {
				test_log_err("Completely incorrect order of events: %d -> %d\n",
				             event_0->event_type, event_1->event_type);
				return -1;
			}
		}
	}




	/* Expectation 3: the events should be separated by close time span. */
	/* Maximal allowed time span between the two events. Currently (0.12.0)
	   the time span is ~300ms.
	   TODO acerion 2023.12.31: shorten the time span. */
	const long int thresh = (long) 500 * 1000 * 1000;

	struct timespec diff = { 0 };
	timespec_diff(&event_0->tstamp, &event_1->tstamp, &diff);
	if (diff.tv_sec == 0 && diff.tv_nsec < thresh) {
		; /* Pass. */
	} else {
		test_log_err("Time difference between end of Morse receive and receiving a reply is too large: %ld:%ld\n",
		             diff.tv_sec, diff.tv_nsec);
		return -1;
	}




	/* Expectation 4: text received by Morse receiver must match input text from test case.

	   While this is not THE feature that needs to be verified by this test,
	   it's good to know that we received full and correct data. */
	{
		/* TODO acerion 2023.12.31: move comparing of received and expected
		   string to some library function. */
		const char * received_string = morse_event->u.morse_receive.string;
		const char * needle = strcasestr(received_string, test_case->message);
		if (NULL == needle) {
			test_log_err("Received incorrect Morse message: expected [%s], received [%s]\n",
			             test_case->message, received_string);
			return -1;
		} else {
			test_log_info("Received expected Morse message: expected [%s], received [%s]\n",
			              test_case->message, received_string);
			; /* Pass. */
		}
	}




	/* Expectation 5: text received in socket message must match text sent in <ESC>h
	   request. */
	{
		const char * actual_raw = socket_event->u.socket_receive.string;

		char expected_raw[64] = { 0 };
		snprintf(expected_raw, sizeof (expected_raw), "h%s\r\n", test_case->requested_reply_value);

		char escaped_expected[64] = { 0 };
		char escaped_actual[64] = { 0 };
		escape_string(expected_raw, escaped_expected, sizeof (escaped_expected));
		escape_string(actual_raw, escaped_actual, sizeof (escaped_actual));

		if (0 != strcmp(expected_raw, actual_raw)) {
			test_log_err("Received incorrect message in socket reply: expected [%s], received [%s]\n",
			             escaped_expected, escaped_actual);
			return -1;
		} else {
			test_log_info("Received expected message in socket reply: expected [%s], received [%s]\n",
			              escaped_expected, escaped_actual);
			; /* Pass. */
		}
	}


	/* Evaluation found no errors. */
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
		.tone           = 700,
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



		thread_t socket_receiver_thread = { .name = "socket receiver thread", .thread_fn = socket_receiver_thread_fn, .thread_fn_arg = &client };
		thread_t morse_receiver_thread  = { .name = "Morse receiver thread", .thread_fn = morse_receiver_thread_fn };



		/* Prepare local test instance of cwdaemon server. */
		if (0 != cwdaemon_start_and_connect(&server_opts, &cwdaemon, &client)) {
			fprintf(stderr, "[EE] Failed to start cwdaemon server, terminating\n");
			failure = true;
			goto cleanup;
		}



		if (0 != thread_start(&morse_receiver_thread)) {
			fprintf(stderr, "[EE] Failed to start Morse receiver thread\n");
			failure = true;
			goto cleanup;
		}
		if (0 != thread_start(&socket_receiver_thread)) {
			fprintf(stderr, "[EE] Failed to start socket receiver thread\n");
			failure = true;
			goto cleanup;
		}



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


		thread_join(&socket_receiver_thread);
		thread_join(&morse_receiver_thread);



		/* For debugging only. */
		events_print(&g_events);

		/* Validation of test run. */
		if (0 != events_evaluate(&g_events, test_case)) {
			fprintf(stderr, "[EE] Test failure: problem with collected events\n");
			failure = true;
			goto cleanup;
		}



	cleanup:
		events_clear(&g_events);

		thread_cleanup(&socket_receiver_thread);
		thread_cleanup(&morse_receiver_thread);

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



