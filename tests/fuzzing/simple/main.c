/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
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
 * GNU Library General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   @file

   Use cwdaemon client to send different random requests to server.

   See what breaks in the server.
*/




#define _DEFAULT_SOURCE




#include "config.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libcw.h>

#include "src/cwdaemon.h"

#include "tests/library/client.h"
#include "tests/library/events.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/server.h"
#include "tests/library/sleep.h"
#include "tests/library/socket.h"
#include "tests/library/string_utils.h"
#include "tests/library/supervisor.h"
#include "tests/library/test_env.h"
#include "tests/library/time_utils.h"




/**
   A value of escaped request is an array that does not include <ESC>, or
   escaped request code.
*/
#define ESC_REQ_VALUE_BUFFER_SIZE (CLIENT_SEND_BUFFER_SIZE - 1 - 1)




/**
   Size of buffer for a value of plain message or caret message.
*/
#define MESSAGE_VALUE_BUFFER_SIZE (CLIENT_SEND_BUFFER_SIZE)





typedef struct test_case_t {
	const char * description;     /** Human-readable description of the test case. */
	int (* fn)(client_t * client, morse_receiver_t * morse_receiver);
} test_case_t;



static int test_fn_esc_reset(client_t * client, morse_receiver_t * morse_receiver);
static int test_fn_esc_speed(client_t * client, morse_receiver_t * morse_receiver);
static int test_fn_esc_tone(client_t * client, morse_receiver_t * morse_receiver);
static int test_fn_esc_abort(client_t * client, morse_receiver_t * morse_receiver);
static int test_fn_esc_exit(client_t * client, morse_receiver_t * morse_receiver);
static int test_fn_esc_word_mode(client_t * client, morse_receiver_t * morse_receiver);
static int test_fn_esc_weight(client_t * client, morse_receiver_t * morse_receiver);
static int test_fn_esc_cwdevice(client_t * client, morse_receiver_t * morse_receiver);
static int test_fn_esc_port(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver);
static int test_fn_esc_ptt_state(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver);
static int test_fn_esc_ssb_way(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver);
static int test_fn_esc_tune(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver);
static int test_fn_esc_tx_delay(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver);
static int test_fn_esc_band_switch(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver);
static int test_fn_esc_sound_system(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver);
static int test_fn_esc_volume(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver);
static int test_fn_esc_reply(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver);
static int test_fn_esc_almost_all(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver);

static int test_fn_plain_message(client_t * client, morse_receiver_t * morse_receiver);
static int test_fn_caret_message(client_t * client, morse_receiver_t * morse_receiver);




static const test_case_t g_test_cases[] = {
#if 1
	{ .description = "esc request - reset",           .fn = test_fn_esc_reset           }, // CWDAEMON_ESC_REQUEST_RESET
	{ .description = "esc request - speed",           .fn = test_fn_esc_speed           }, // CWDAEMON_ESC_REQUEST_SPEED
	{ .description = "esc request - tone",            .fn = test_fn_esc_tone            }, // CWDAEMON_ESC_REQUEST_TONE
	{ .description = "esc request - abort",           .fn = test_fn_esc_abort           }, // CWDAEMON_ESC_REQUEST_ABORT
	{ .description = "esc request - exit",            /* .fn = test_fn_esc_exit */      }, // CWDAEMON_ESC_REQUEST_EXIT
	{ .description = "esc request - word mode",       .fn = test_fn_esc_word_mode       }, // CWDAEMON_ESC_REQUEST_WORD_MODE
	{ .description = "esc request - weighting",       .fn = test_fn_esc_weight          }, // CWDAEMON_ESC_REQUEST_WEIGHTING
	{ .description = "esc request - cwdevice",        .fn = test_fn_esc_cwdevice        }, // CWDAEMON_ESC_REQUEST_CWDEVICE
	{ .description = "esc request - port",            .fn = test_fn_esc_port            }, // CWDAEMON_ESC_REQUEST_PORT
	{ .description = "esc request - ptt state",       .fn = test_fn_esc_ptt_state       }, // CWDAEMON_ESC_REQUEST_PTT_STATE
	{ .description = "esc request - ssb way",         .fn = test_fn_esc_ssb_way         }, // CWDAEMON_ESC_REQUEST_SSB_WAY
	{ .description = "esc request - tune",            .fn = test_fn_esc_tune            }, // CWDAEMON_ESC_REQUEST_TUNE
	{ .description = "esc request - tx delay",        .fn = test_fn_esc_tx_delay        }, // CWDAEMON_ESC_REQUEST_TX_DELAY
	{ .description = "esc request - band switch",     .fn = test_fn_esc_band_switch     }, // CWDAEMON_ESC_REQUEST_BAND_SWITCH
	{ .description = "esc request - sound system",    .fn = test_fn_esc_sound_system    }, // CWDAEMON_ESC_REQUEST_SOUND_SYSTEM
	{ .description = "esc request - volume",          .fn = test_fn_esc_volume          }, // CWDAEMON_ESC_REQUEST_VOLUME
#endif
#if 1
	{ .description = "esc request - reply",           .fn = test_fn_esc_reply           }, // CWDAEMON_ESC_REQUEST_REPLY
	{ .description = "esc request - reply",           .fn = test_fn_esc_reply           }, // CWDAEMON_ESC_REQUEST_REPLY
	{ .description = "esc request - reply",           .fn = test_fn_esc_reply           }, // CWDAEMON_ESC_REQUEST_REPLY
#endif
#if 1
	{ .description = "esc request - almost all",      .fn = test_fn_esc_almost_all      },
#endif
#if 1
	{ .description = "plain message",                 .fn = test_fn_plain_message },
	{ .description = "plain message",                 .fn = test_fn_plain_message },
	{ .description = "plain message",                 .fn = test_fn_plain_message },

	{ .description = "caret message",                 .fn = test_fn_caret_message },
	{ .description = "caret message",                 .fn = test_fn_caret_message },
	{ .description = "caret message",                 .fn = test_fn_caret_message },
#endif
};




static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_run(const test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver);




int main(void)
{
#if 0
	if (!test_env_is_usable(test_env_libcw_without_signals)) {
		test_log_err("Test: preconditions for test env are not met, exiting %s\n", "");
		exit(EXIT_FAILURE);
	}
#endif

	const uint32_t seed = cwdaemon_srandom(0);
	test_log_debug("Test: random seed: 0x%08x (%u)\n", seed, seed);

	bool failure = false;
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
	server_t server = { .events = &events };
	client_t client = { .events = &events };
	morse_receiver_t morse_receiver = { .events = &events };

	if (0 != test_setup(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at test setup %s\n", "");
		failure = true;
		goto cleanup;
	}

	if (test_run(g_test_cases, n_test_cases, &client, &morse_receiver)) {
		test_log_err("Test: failed at running test cases %s\n", "");
		failure = true;
		goto cleanup;
	}

 cleanup:
	if (0 != test_teardown(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at test tear down %s\n", "");
		failure = true;
	}

	test_log_newline(); /* Visual separator. */
	if (failure) {
		test_log_err("Test: the test has failed %s\n", "");
		exit(EXIT_FAILURE);
	} else {
		test_log_info("Test: the test has passed %s\n", "");
		exit(EXIT_SUCCESS);
	}
}



/**
   @brief Prepare resources used to execute set of test cases
*/
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver)
{
	bool failure = false;

	const int wpm = 40;

	/* Prepare local test instance of cwdaemon server. */
	cwdaemon_opts_t server_opts = {
		.supervisor_id =  supervisor_id_valgrind,
		.tone           = test_get_test_tone(),
		.sound_system   = CW_AUDIO_NULL,
		.nofork         = true,
		.cwdevice_name  = TEST_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
	};
	if (0 != server_start(&server_opts, server)) {
		test_log_err("Test: tailed to start cwdaemon server %s\n", "");
		failure = true;
	}


	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.01.27: remove casting. */
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		failure = true;
	}
	client_socket_receive_enable(client);
	if (0 != client_socket_receive_start(client)) {
		test_log_err("Test: failed to start socket receiver %s\n", "");
		failure = true;
	}


	const morse_receiver_config_t morse_config = { .wpm = wpm };
	if (0 != morse_receiver_ctor(&morse_config, morse_receiver)) {
		test_log_err("Test: failed to create Morse receiver %s\n", "");
		failure = true;
	}


	return failure ? -1 : 0;
}




/**
   @brief Clean up resources used to execute set of test cases
*/
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver)
{
	bool failure = false;

	/* Terminate local test instance of cwdaemon server. Always do it first
	   since the server is the main trigger of events in the test. */
	if (0 != local_server_stop(server, client)) {
		/*
		  Stopping a server is not a main part of a test, but if a
		  server can't be closed then it means that the main part of the
		  code has left server in bad condition. The bad condition is an
		  indication of an error in tested functionality. Therefore set
		  failure to true.
		*/
		test_log_err("Test: failed to correctly stop local test instance of cwdaemon %s\n", "");
		failure = true;
	}

	morse_receiver_dtor(morse_receiver);

	client_socket_receive_stop(client);
	client_disconnect(client);
	client_dtor(client);

	return failure ? -1 : 0;
}




/**
   @brief Run all test cases. Evaluate results (the events) of each test case.
*/
static int test_run(const test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver)
{
	bool failure = false;


	const size_t n_iters = 40;
	for (size_t iter = 0; iter < n_iters; ) {

		unsigned int lower = 0;
		unsigned int upper = (unsigned int) n_test_cases - 1;
		unsigned int test_case_number = 0;
		cwdaemon_random_uint(lower, upper, &test_case_number);

		const test_case_t * test_case = &test_cases[test_case_number];

		if (!test_case->fn) {
			continue;
		}

		test_log_newline(); /* Visual separator. */
		test_log_info("Test: starting test case %u / %zu [%s] in iter %zu / %zu\n",
		              test_case_number + 1, n_test_cases,
		              test_case->description,
		              iter + 1, n_iters);

		if (0 != test_case->fn(client, morse_receiver)) {
			test_log_info("Test: test case has failed %s\n", "");
			failure = true;
			break;
		}
		test_log_info("Test: test case %u / %zu succeeded\n\n", test_case_number + 1, n_test_cases);
		/* Only test cases with real test functionality shall move
		   test count forward. */
		iter++;
	}

	return failure ? -1 : 0;
}




typedef enum value_mode_t {
	value_mode_empty,          /* Return array of bytes that can be interpreted as empty C string. */
	value_mode_in_range,       /* Return array of bytes that can be interpreted as string representation of a random value that is in range of valid values. */
	value_mode_random_bytes,   /* Return array of completely random bytes. */
} value_mode_t;


static int get_value_mode(value_mode_t * value_mode)
{
	unsigned int lower = (unsigned int) value_mode_empty;
	unsigned int upper = (unsigned int) value_mode_random_bytes;
	unsigned int x = 0;
	cwdaemon_random_uint(lower, upper, &x);
	*value_mode = (value_mode_t) x;

	switch (*value_mode) {
	case value_mode_empty:
		test_log_info("Test: value mode = EMPTY %s\n", "");
		break;
	case value_mode_in_range:
		test_log_info("Test: value mode = IN RANGE %s\n", "");
		break;
	case value_mode_random_bytes:
		test_log_info("Test: value mode = RANDOM BYTES %s\n", "");
		break;
	default:
		test_log_err("Test: value mode = UNKNOWN: %d\n", *value_mode);
		break;
	}

	return 0;
}




/**
   Pick a size of data to be sent by cwdaemon client

   Usually we want to send exactly as many bytes as there are in generated
   value (i.e. @p value_size), but sometimes we may want to send more or less.

   The function can decide to return any value between zero and @p buf_size,
   which represents total size of buffer in which value is stored.

   @p buf_size is the upper limit. @p value_size <= @p buf_size.

   @param value_size Count of bytes that represent a generated value
   @param buf_size Count of bytes that can be stored in value buffer

   @return Count of bytes that should be used as size of data to be sent by cwdaemon client
 */
static size_t get_n_bytes_to_send(int value_size, size_t buf_size)
{
	size_t n_bytes_to_send = value_size;

	/* Randomly switch between using exact @p value_size and some random
	   value. But be biased towards using @p value_size. */
	bool use_value_size = true;
	cwdaemon_random_biased_bool(10, &use_value_size); /* antibias = 10 -> not very biased towards returning 'true'. */

	if (!use_value_size) {
		unsigned int lower = 0;
		unsigned int upper = (unsigned int) buf_size;
		unsigned int val = 0;
		cwdaemon_random_uint(lower, upper, &val);

		n_bytes_to_send = (size_t) val;
	}

	test_log_debug("Test: size of value = %d bytes, n bytes to send = %zu (%s)\n",
	               value_size, n_bytes_to_send, use_value_size ? "used value size" : "used random size");

	return n_bytes_to_send;
}




/**
   @return count of bytes put into buffer on success
   @return -1 on error
*/
static int get_value_string_boolean(char * buffer, size_t size)
{
	bool value_in_range = false;
	bool truly_empty = true;

	value_mode_t value_mode = value_mode_empty;
	get_value_mode(&value_mode);
	switch (value_mode) {
	case value_mode_empty:
		cwdaemon_random_bool(&truly_empty);
		if (truly_empty) {
			memset(buffer, 0, size);
			return 0;
		} else {
			buffer[0] = '\0';
			return 1; /* One byte. */
		}
	case value_mode_in_range:
		cwdaemon_random_bool(&value_in_range);
		snprintf(buffer, size, "%d", (int) value_in_range);
		return (int) (strlen(buffer) + 1);
	case value_mode_random_bytes:
		if (0 != cwdaemon_random_bytes(buffer, size)) {
			test_log_err("Test: failed to get random bytes in %s\n", __func__);
			return -1;
		}
		return (int) size;
	default:
		test_log_err("Test: uhandled value mode %d in %s\n", (int) value_mode, __func__);
		return -1;
	}
}




/**
   @return count of bytes put into buffer on success
   @return -1 on error
*/
static int get_value_string_int(char * buffer, size_t size, int range_low, int range_high)
{
	unsigned int value_in_range = 0;
	bool truly_empty = true;

	value_mode_t value_mode = value_mode_empty;
	get_value_mode(&value_mode);
	switch (value_mode) {
	case value_mode_empty:
		cwdaemon_random_bool(&truly_empty);
		if (truly_empty) {
			memset(buffer, 0, size);
			return 0;
		} else {
			buffer[0] = '\0';
			return 1; /* One byte. */
		}
	case value_mode_in_range:
		cwdaemon_random_uint((unsigned int) range_low, (unsigned int) range_high, &value_in_range);
		snprintf(buffer, size, "%u", value_in_range);
		return (int) (strlen(buffer) + 1);
	case value_mode_random_bytes:
		if (0 != cwdaemon_random_bytes(buffer, size)) {
			test_log_err("Test: failed to get random bytes in %s\n", __func__);
			return -1;
		}
		return (int) size;
	default:
		test_log_err("Test: unhandled value mode %d in %s\n", (int) value_mode, __func__);
		return -1;
	}
}




/**
   @return count of bytes put into buffer on success
   @return -1 on error
*/
static int get_value_string_string(char * buffer, size_t size)
{
	bool truly_empty = true;

	value_mode_t value_mode = value_mode_empty;
	get_value_mode(&value_mode);
	switch (value_mode) {
	case value_mode_empty:
		cwdaemon_random_bool(&truly_empty);
		if (truly_empty) {
			memset(buffer, 0, size);
			return 0;
		} else {
			buffer[0] = '\0';
			return 1; /* One byte. */
		}
	case value_mode_in_range:
		snprintf(buffer, size, "%s", "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee_abcdefghijklmnopqrstuvw");
		return (int) (strlen(buffer) + 1);
	case value_mode_random_bytes:
		if (0 != cwdaemon_random_bytes(buffer, size)) {
			test_log_err("Test: failed to get random bytes in %s\n", __func__);
			return -1;
		}
		return (int) size;
	default:
		test_log_err("Test: unhandled value mode %d in %s\n", (int) value_mode, __func__);
		return -1;
	}
}



static int test_fn_esc_reset(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	/*
	  This request doesn't require a value, but we will try to send one
	  anyway. Just get any byte array as value for a request that doesn't
	  expect any value.
	*/
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_string(value, sizeof (value));
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_RESET, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'RESET' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(2);

	return 0;
}




static int test_fn_esc_speed(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_int(value, sizeof (value), CW_SPEED_MIN, CW_SPEED_MAX);
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_SPEED, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'SPEED' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}




static int test_fn_esc_tone(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_int(value, sizeof (value), CW_FREQUENCY_MIN, CW_FREQUENCY_MAX);
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_TONE, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'TONE' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}




static int test_fn_esc_abort(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	/*
	  This request doesn't require a value, but we will try to send one
	  anyway. Just get any byte array as value for a request that doesn't
	  expect any value.
	*/
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_string(value, sizeof (value));
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_ABORT, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'ABORT' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}





__attribute__((unused)) static int test_fn_esc_exit(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	/*
	  This request doesn't require a value, but we will try to send one
	  anyway. Just get any byte array as value for a request that doesn't
	  expect any value.
	*/
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_string(value, sizeof (value));
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_EXIT, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'EXIT' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}




static int test_fn_esc_word_mode(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_boolean(value, sizeof (value));
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_WORD_MODE, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'WORD MODE' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}




static int test_fn_esc_weight(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_int(value, sizeof (value), 0, CWDAEMON_MORSE_WEIGHTING_MAX); /* TODO: lower range (the 3rd arg) should be CWDAEMON_MORSE_WEIGHTING_MIN. */
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_WEIGHTING, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'WEIGHTING' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}




/*
  TODO (acerion) 2024.02.11: implement properly: "cwdevice" may require some
  especial cases for values.
 */
static int test_fn_esc_cwdevice(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_string(value, sizeof (value));
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_CWDEVICE, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'CWDEVICE' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(2);

	return 0;
}



static int test_fn_esc_port(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_int(value, sizeof (value), CWDAEMON_NETWORK_PORT_MIN, CWDAEMON_NETWORK_PORT_MAX);
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_PORT, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'PORT' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}




static int test_fn_esc_ptt_state(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_boolean(value, sizeof (value));
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_PTT_STATE, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'PTT STATE' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}




static int test_fn_esc_ssb_way(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_int(value, sizeof (value), 0, 1);
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_SSB_WAY, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'SSB WAY' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}




static int test_fn_esc_tx_delay(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_int(value, sizeof (value), CWDAEMON_PTT_DELAY_MIN, CWDAEMON_PTT_DELAY_MAX);
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_TX_DELAY, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'TX DELAY' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}




static int test_fn_esc_tune(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_int(value, sizeof (value), 0, 10); /* TODO acerion 2024.03.01: replace magic values with constants. */
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_TUNE, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'TUNE' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}




static int test_fn_esc_band_switch(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_int(value, sizeof (value), 0, INT_MAX); /* TODO acerion 2024.03.01: use correct values to specify range of valid values. */
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_BAND_SWITCH, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'BAND SWITCH' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}




/*
  TODO (acerion) 2024.02.11: implement properly: "sound system" may require
  some especial cases for values.
 */
static int test_fn_esc_sound_system(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_string(value, sizeof (value));
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_SOUND_SYSTEM, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'SOUND SYSTEM' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(2);

	return 0;
}




static int test_fn_esc_volume(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_int(value, sizeof (value), CW_VOLUME_MIN, CW_VOLUME_MAX);
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_VOLUME, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send 'VOLUME' escaped request %s\n", "");
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}



static int test_fn_esc_reply(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	{
		char requested_reply[ESC_REQ_VALUE_BUFFER_SIZE] = { 0 };
		const int value_size = get_value_string_string(requested_reply, sizeof (requested_reply));
		if (value_size < 0) {
			test_log_err("Test: failed to generate value string %s\n", "");
			return -1;
		}

		const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (requested_reply));
		if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_REPLY, requested_reply, n_bytes_to_send)) {
			test_log_err("Test: failed to send REPLY escaped request %s\n", "");
			return -1;
		}
	}

	{
		char message[MESSAGE_VALUE_BUFFER_SIZE] = { 0 };
		const int value_size = get_value_string_string(message, sizeof (message));
		if (value_size < 0) {
			test_log_err("Test: failed to generate value string %s\n", "");
			return -1;
		}

		const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (message));
		if (0 != client_send_message(client, message, n_bytes_to_send)) {
			test_log_err("Test: failed to send PLAIN MESSAGE in tests of REPLY escaped request %s\n", "");
			return -1;
		}

		/* At 40 wpm it takes ~30 seconds to play string consisting of 254 'e' characters. */
		test_sleep_nonintr(40);
	}

	/* TODO acerion 2024.03.01: add here receiving of reply. */

	return 0;
}




/**
   @brief Send escape requests with (almost) all values of escape codes

   Function sends escape requests that contain code values from full range of
   'char' type. The function is meant to test behaviour of cwdaaemon when
   unsupported escape requests are being sent.

   Of course I could wait for some other "random" functions to generate all
   possible (valid and invalid) escape requests, but that would take
   bazillion of iterations. Instead of that I'm using this function that is
   designed to test just that one aspect.

   Each call of the function sends 255 escape requests with 255 values of
   escape code - with exception of EXIT escape request.
*/
static int test_fn_esc_almost_all(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	/* Don't use 'unsigned char' type for 'code'. With this range of values the
	   usage of 'unsigned char' would result in infinite loop. */
	for (unsigned int code = 0; code <= UCHAR_MAX; code++) {
		if ((char) code == CWDAEMON_ESC_REQUEST_EXIT) {
			/* Don't tell cwdaemon to exit in the middle of a test :) */
			continue;
		}

		/* Full message, including leading <ESC> and escape code. */
		char full_request[MESSAGE_VALUE_BUFFER_SIZE] = { 0 };
		const int head = snprintf(full_request, sizeof (full_request), "%c%c", ASCII_ESC, (char) code);

		const int value_size = get_value_string_string(full_request + head, sizeof (full_request) - head);
		if (value_size < 0) {
			test_log_err("Test: failed to generate value for esc request string %s\n", "");
			return -1;
		}

		const size_t n_bytes_to_send = sizeof (full_request);
		if (0 != client_send_message(client, full_request, n_bytes_to_send)) {
			test_log_err("Test: failed to send escape request with code %d / 0x%02x\n", code, (unsigned char) code);
			return -1;
		}

		test_millisleep_nonintr(200);
	}

	return 0;
}




// TODO (acerion) 2024.02.25: implement properly: make sure that messages have varying lengths.
static int test_fn_plain_message(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[MESSAGE_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_string(value, sizeof (value));
	if (value_size < 0) {
		test_log_err("Test: failed to generate value string %s\n", "");
		return -1;
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_message(client, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send PLAIN MESSAGE %s\n", "");
		return -1;
	}

	/* At 40 wpm it takes ~30 seconds to play string consisting of 254 'e' characters. */
	test_sleep_nonintr(40);

	return 0;
}




// TODO (acerion) 2024.02.25: implement properly: make sure that messages have varying lengths.
static int test_fn_caret_message(client_t * client, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	char value[MESSAGE_VALUE_BUFFER_SIZE] = { 0 };
	const int value_size = get_value_string_string(value, sizeof (value));
	if (value_size < 0) {
		test_log_err("Test: failed to get generate value string %s\n", "");
		return -1;
	}

	/* Add some carets. Randomize the position of carets. Leave characters
	   after the first caret intact, see how cwdaemon handles them. */
	for (int i = 0; i < 3; i++) {

		if (i == 0) {
			; /* Always add at least one caret. */
		} else {
			bool add_this_caret = true;
			cwdaemon_random_bool(&add_this_caret);
			if (!add_this_caret) {
				continue;
			}
		}

		unsigned int lower = 0;
		unsigned int upper = strlen(value);
		if (upper) {
			/* Be sure to not select index of value[] that would point to
			   terminating NUL. */
			upper--;
		}
		unsigned int mark = 0;
		cwdaemon_random_uint(lower, upper, &mark);
		if (value[mark] != '\0') { /* Another check for NUL, just in case. */
			value[mark] = '^';
		}
	}

	const size_t n_bytes_to_send = get_n_bytes_to_send(value_size, sizeof (value));
	if (0 != client_send_message(client, value, n_bytes_to_send)) {
		test_log_err("Test: failed to send CARET MESSAGE %s\n", "");
		return -1;
	}

	/* At 40 wpm it takes ~30 seconds to play string consisting of 254 'e' characters. */
	test_sleep_nonintr(40);

	return 0;
}


