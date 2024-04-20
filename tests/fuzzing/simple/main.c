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
 * GNU General Public License for more details.

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
#include "tests/library/test_options.h"
#include "tests/library/time_utils.h"




typedef struct test_case_t {
	const char * description;     /**< Human-readable description of the test case. */
	int (* fn)(client_t * client, const struct test_case_t * test_case, morse_receiver_t * morse_receiver);
	char code;                    /**< Escape request code. */
	int int_min;                  /**< Lower bound of valid values for Escape requests requiring integer argument. */
	int int_max;                  /**< Upper bound of valid values for Escape requests requiring integer argument. */
} test_case_t;




static int test_fn_esc_no_value(client_t * client, const test_case_t * test_case, morse_receiver_t * morse_receiver);
static int test_fn_esc_int(client_t * client, const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver);
static int test_fn_esc_bool(client_t * client, const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver);

static int test_fn_esc_cwdevice(client_t * client, const test_case_t * test_case, morse_receiver_t * morse_receiver);
static int test_fn_esc_sound_system(client_t * client, const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver);
static int test_fn_esc_reply(client_t * client, const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver);
static int test_fn_esc_almost_all(client_t * client, const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver);

static int test_fn_plain_message(client_t * client, const test_case_t * test_case, morse_receiver_t * morse_receiver);
static int test_fn_caret_message(client_t * client, const test_case_t * test_case, morse_receiver_t * morse_receiver);

static int test_request_set_value_string(test_request_t * request);
static int test_request_set_value_bool(test_request_t * request);
static int test_request_set_value_int(test_request_t * request, int range_low, int range_high);
static void test_request_set_random_n_bytes(test_request_t * request);

/**
   Set first byte of request to Esc character. Set second byte of request to
   given Escape request code. Set count of bytes in request to 2, since this
   is how many bytes are being put into the request.
*/
#define TEST_REQUEST_INIT_ESC(_code_) { .bytes[0] = ASCII_ESC, .bytes[1] = (_code_), .n_bytes = 2 }




/*
  TODO acerion 2024.03.24: add a function that sends the following special
  cases:
   - Escape request that consists only of N <ESC> characters (N = 1 - MAX).
   - Requests that contain multiple NUL characters, especially requests that
     consist only of NUL characters.

  Notice that the test function doesn't go over the array in linear way. The
  test function selects test cases in random order.
*/
static const test_case_t g_test_cases[] = {
#if 1
	{ .description = "esc request - reset",           .fn = test_fn_esc_no_value,       .code = CWDAEMON_ESC_REQUEST_RESET,          },
	{ .description = "esc request - speed",           .fn = test_fn_esc_int,            .code = CWDAEMON_ESC_REQUEST_SPEED,         .int_min = CW_SPEED_MIN,     .int_max = CW_SPEED_MAX,     },
	{ .description = "esc request - tone",            .fn = test_fn_esc_int,            .code = CWDAEMON_ESC_REQUEST_TONE,          .int_min = CW_FREQUENCY_MIN, .int_max = CW_FREQUENCY_MAX, },
	{ .description = "esc request - abort",           .fn = test_fn_esc_no_value,       .code = CWDAEMON_ESC_REQUEST_ABORT,          },
	{ .description = "esc request - exit",            /* .fn = test_fn_esc_no_value */  .code = CWDAEMON_ESC_REQUEST_EXIT,           },
	{ .description = "esc request - word mode",       .fn = test_fn_esc_bool,           .code = CWDAEMON_ESC_REQUEST_WORD_MODE,      },
	{ .description = "esc request - weighting",       .fn = test_fn_esc_int,            .code = CWDAEMON_ESC_REQUEST_WEIGHTING,     .int_min = 0,                         .int_max = CWDAEMON_MORSE_WEIGHTING_MAX, }, 	/* TODO acerion 2024.03.24: .int_min should be CWDAEMON_MORSE_WEIGHTING_MIN. */
	{ .description = "esc request - cwdevice",        .fn = test_fn_esc_cwdevice,       .code = CWDAEMON_ESC_REQUEST_CWDEVICE,       },
	{ .description = "esc request - port",            .fn = test_fn_esc_int,            .code = CWDAEMON_ESC_REQUEST_PORT,          .int_min = CWDAEMON_NETWORK_PORT_MIN, .int_max = CWDAEMON_NETWORK_PORT_MAX, },
	{ .description = "esc request - ptt state",       .fn = test_fn_esc_bool,           .code = CWDAEMON_ESC_REQUEST_PTT_STATE,      },
	{ .description = "esc request - ssb way",         .fn = test_fn_esc_int,            .code = CWDAEMON_ESC_REQUEST_SSB_WAY,       .int_min = 0,                      .int_max = 1,  },
	{ .description = "esc request - tune",            .fn = test_fn_esc_int,            .code = CWDAEMON_ESC_REQUEST_TUNE,          .int_min = 0,                      .int_max = 10, },   /* TODO acerion 2024.03.01: replace magic values with constants. */
	{ .description = "esc request - tx delay",        .fn = test_fn_esc_int,            .code = CWDAEMON_ESC_REQUEST_TX_DELAY,      .int_min = CWDAEMON_PTT_DELAY_MIN, .int_max = CWDAEMON_PTT_DELAY_MAX, },
	{ .description = "esc request - band switch",     .fn = test_fn_esc_int,            .code = CWDAEMON_ESC_REQUEST_BAND_SWITCH,   .int_min = 0,                      .int_max = INT_MAX, }, 	/* TODO acerion 2024.03.01: use correct values to specify range of valid values. */
	{ .description = "esc request - sound system",    .fn = test_fn_esc_sound_system,   .code = CWDAEMON_ESC_REQUEST_SOUND_SYSTEM,   },
	{ .description = "esc request - volume",          .fn = test_fn_esc_int,            .code = CWDAEMON_ESC_REQUEST_VOLUME,        .int_min = CW_VOLUME_MIN,          .int_max = CW_VOLUME_MAX, },
#endif
#if 1
	{ .description = "esc request - reply",           .fn = test_fn_esc_reply,          .code = CWDAEMON_ESC_REQUEST_REPLY, },
	{ .description = "esc request - reply",           .fn = test_fn_esc_reply,          .code = CWDAEMON_ESC_REQUEST_REPLY, },
	{ .description = "esc request - reply",           .fn = test_fn_esc_reply,          .code = CWDAEMON_ESC_REQUEST_REPLY, },
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




static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_options_t * test_opts);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_run(const test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver);




int main(int argc, char * const * argv)
{
	if (!testing_env_is_usable(testing_env_libcw_without_signals)) {
		test_log_err("Test: preconditions for testing env are not met, exiting %s\n", "");
		exit(EXIT_FAILURE);
	}

	test_options_t test_opts = { .sound_system = CW_AUDIO_NULL, .supervisor_id = supervisor_id_valgrind };
	if (0 != test_options_get(argc, argv, &test_opts)) {
		test_log_err("Test: failed to process command line options %s\n", "");
		exit(EXIT_FAILURE);
	}
	if (test_opts.invoked_help) {
		/* Help text was printed as requested. Now exit. */
		exit(EXIT_SUCCESS);
	}

	const uint32_t seed = cwdaemon_srandom(test_opts.random_seed);
	test_log_debug("Test: random seed: 0x%08x (%u)\n", seed, seed);

	bool failure = false;
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
	server_t server = { .events = &events };
	client_t client = { .events = &events };
	morse_receiver_t morse_receiver = { .events = &events };

	if (0 != test_setup(&server, &client, &morse_receiver, &test_opts)) {
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
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_options_t * test_opts)
{
	bool failure = false;

	const int wpm = 40;

	/* Prepare local test instance of cwdaemon server. */
	server_options_t server_opts = {
		.tone           = tests_get_test_tone(),
		.sound_system   = test_opts->sound_system,
		.nofork         = true,
		.cwdevice_name  = TESTS_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
		.supervisor_id =  test_opts->supervisor_id,
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

	/*
	  Terminate local test instance of cwdaemon server. Always do it first
	  since the server is the main trigger of events in the test.

	  The third arg to the 'stop' function is 'true' because we want to fuzz
	  the daemon till the very end.

	  This entire test is sending many requests multiple times, but EXIT esc
	  request is sent only once.
	*/
	if (0 != local_server_stop_fuzz(server, client, true)) {
		/*
		  If a server can't be closed then it means that the main part of the
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

		const unsigned int lower = 0;
		const unsigned int upper = (unsigned int) n_test_cases - 1;
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

		if (0 != test_case->fn(client, test_case, morse_receiver)) {
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
	const unsigned int lower = (unsigned int) value_mode_empty;
	const unsigned int upper = (unsigned int) value_mode_random_bytes;
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
		test_log_err("Test: value mode = UNKNOWN: %u\n", *value_mode);
		break;
	}

	return 0;
}




/**
   Pick a size of data to be sent by cwdaemon client

   Usually we want to send exactly as many bytes as there are in generated
   request (i.e. @p request->n_bytes), but sometimes we may want to send more or less.

   The function can decide to set some random value of request->n_bytes which
   represents total count of bytes in request.

   If given request is an Escape request, the lower limit on the random value
   is 2 (to not truncate the Escape requests' header).

   The function decides to change the value of n_bytes at random, meaning
   that sometimes the function doesn't change the value.

   TODO acerion 2024.03.28: the function should slightly prioritize sizes
   that are close to size of cwdaemon's receive buffer. The reason for this
   is: I would like this test to be able off-by-one errors in cwdaemon's code
   handling requests and replies.

   @param[in/out] request Request in which to set n_bytes value
*/
static void test_request_set_random_n_bytes(test_request_t * request)
{
	/* Change the value of n_bytes only sometimes, at random. */
	bool keep_value_size = true;
	cwdaemon_random_biased_towards_false(10, &keep_value_size); /* bias = 10 -> very biased towards returning 'false'. */

	if (!keep_value_size) {
		const unsigned int lower = request->bytes[0] == ASCII_ESC ? 2 : 0;
		const unsigned int upper = (unsigned int) sizeof (request->bytes);
		unsigned int val = 0;
		cwdaemon_random_uint(lower, upper, &val);

		request->n_bytes = (size_t) val;

		test_log_debug("Test: count of bytes in value has been selected at random: %zu\n", request->n_bytes);
	}

	return;
}




/**
   @return 0 on success
   @return -1 on error
*/
static int test_request_set_value_bool(test_request_t * request)
{
	bool value_in_range = false;
	bool truly_empty = true;

	/* Skip potential header of Escape request (the Escape character + Escape
	   code). */
	char * const val_start = request->bytes + request->n_bytes;
	const size_t val_size = sizeof (request->bytes) - request->n_bytes;

	value_mode_t value_mode = value_mode_empty;
	get_value_mode(&value_mode);
	switch (value_mode) {
	case value_mode_empty:
		cwdaemon_random_bool(&truly_empty);
		if (truly_empty) {
			memset(val_start, 0, val_size);
			return 0;
		} else {
			val_start[0] = '\0';
			request->n_bytes += 1; /* One byte: '\0'. */
			return 0;
		}
	case value_mode_in_range:
		cwdaemon_random_bool(&value_in_range);
		snprintf(val_start, val_size, "%u", (unsigned int) value_in_range);
		request->n_bytes += strlen(val_start) + 1;
		return 0;
	case value_mode_random_bytes:
		if (0 != cwdaemon_random_bytes(val_start, val_size)) {
			test_log_err("Test: failed to get random bytes in %s\n", __func__);
			return -1;
		}
		request->n_bytes += val_size;
		return 0;
	default:
		test_log_err("Test: unhandled value mode %u in %s\n", value_mode, __func__);
		return -1;
	}
}




/**
   @return 0 on success
   @return -1 on error
*/
static int test_request_set_value_int(test_request_t * request, int range_low, int range_high)
{
	unsigned int value_in_range = 0;
	bool truly_empty = true;

	/* Skip potential header of Escape request (the Escape character + Escape
	   code). */
	char * const val_start = request->bytes + request->n_bytes;
	const size_t val_size = sizeof (request->bytes) - request->n_bytes;

	value_mode_t value_mode = value_mode_empty;
	get_value_mode(&value_mode);
	switch (value_mode) {
	case value_mode_empty:
		cwdaemon_random_bool(&truly_empty);
		if (truly_empty) {
			memset(val_start, 0, val_size);
			return 0;
		} else {
			val_start[0] = '\0';
			request->n_bytes += 1; /* One byte: '\0'. */
			return 0;
		}
	case value_mode_in_range:
		// TODO (acerion) 2024.04.19: cwdaemon's weighting option accepts
		// also negative values, so this call to _uint() doesn't cover all
		// range. Use _int() function?
		cwdaemon_random_uint((unsigned int) range_low, (unsigned int) range_high, &value_in_range);
		snprintf(val_start, val_size, "%u", value_in_range);
		request->n_bytes += strlen(val_start) + 1;
		return 0;
	case value_mode_random_bytes:
		if (0 != cwdaemon_random_bytes(val_start, val_size)) {
			test_log_err("Test: failed to get random bytes in %s\n", __func__);
			return -1;
		}
		request->n_bytes += val_size;
		return 0;
	default:
		test_log_err("Test: unhandled value mode %u in %s\n", value_mode, __func__);
		return -1;
	}
}




/**
   @return 0 on success
   @return -1 on error
*/
static int test_request_set_value_string(test_request_t * request)
{
	bool truly_empty = true;

	/* Skip potential header of Escape request (the Escape character + Escape
	   code). */
	char * const val_start = request->bytes + request->n_bytes;
	const size_t val_size = sizeof (request->bytes) - request->n_bytes;

	value_mode_t value_mode = value_mode_empty;
	get_value_mode(&value_mode);
	switch (value_mode) {
	case value_mode_empty:
		cwdaemon_random_bool(&truly_empty);
		if (truly_empty) {
			memset(val_start, 0, val_size);
			return 0;
		} else {
			val_start[0] = '\0';
			request->n_bytes += 1; /* One byte: '\0'. */
			return 0;
		}
	case value_mode_in_range:
		snprintf(val_start, val_size, "%s", "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee_abcdefghijklmnopqrstuvw");
		request->n_bytes += strlen(val_start) + 1;
		return 0;
	case value_mode_random_bytes:
		if (0 != cwdaemon_random_bytes(val_start, val_size)) {
			test_log_err("Test: failed to get random bytes in %s\n", __func__);
			return -1;
		}
		request->n_bytes += val_size;
		return 0;
	default:
		test_log_err("Test: unhandled value mode %u in %s\n", value_mode, __func__);
		return -1;
	}
}




/*
*/
/**
   @brief Send Escape request that doesn't require a value

   This Escape request doesn't require a value, but we will try to send one
   anyway to stress-test cwdaemon. The function will just get an array of
   random bytes and use it as value.

   Escape request code will be taken from @p test_case.

   @param client cwdaemon client to use for sending of the request
   @param[in] test_case Current test case
   @param morse_receiver Morse receiver that may or may not be used to receive Morse code on cwdevice during additional verification

   @return 0 on success
   @return -1 on failure
*/
static int test_fn_esc_no_value(client_t * client, const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	test_request_t request = TEST_REQUEST_INIT_ESC(test_case->code);
	if (0 != test_request_set_value_string(&request)) {
		test_log_err("Test: failed to set value of request in test case [%s]\n", test_case->description);
		return -1;
	}
	test_request_set_random_n_bytes(&request);

	if (0 != client_send_request(client, &request)) {
		test_log_err("Test: failed to send Escape request in test case [%s]\n", test_case->description);
		return -1;
	}

	test_sleep_nonintr(2); /* TODO acerion 2024.02.24: for RESET the sleep should be longer. For others it should be shorter. */

	return 0;
}




/**
   @brief Send Escape request that requires an integer value

   The integer will be sent as its string representation.

   Escape request code will be taken from @p test_case.

   Lower and upper bound of range of valid integer values will be taken from @p test_case.

   @param client cwdaemon client to use for sending of the request
   @param[in] test_case Current test case
   @param morse_receiver Morse receiver that may or may not be used to receive Morse code on cwdevice during additional verification

   @return 0 on success
   @return -1 on failure
*/
static int test_fn_esc_int(client_t * client, const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	test_request_t request = TEST_REQUEST_INIT_ESC(test_case->code);
	if (0 != test_request_set_value_int(&request, test_case->int_min, test_case->int_max)) {
		test_log_err("Test: failed to set value of request in test case [%s]\n", test_case->description);
		return -1;
	}
	test_request_set_random_n_bytes(&request);

	if (0 != client_send_request(client, &request)) {
		test_log_err("Test: failed to send Escape request in test case [%s]\n", test_case->description);
		return -1;
	}

	test_sleep_nonintr(1); /* TODO acerion 2024.03.24: sleep time for "tune" should depend on time of tuning. */

	return 0;
}




/**
   @brief Send Escape request that requires a boolean value

   The boolean will be sent as its string representation.

   Escape request code will be taken from @p test_case.

   @param client cwdaemon client to use for sending of the request
   @param[in] test_case Current test case
   @param morse_receiver Morse receiver that may or may not be used to receive Morse code on cwdevice during additional verification

   @return 0 on success
   @return -1 on failure
*/
static int test_fn_esc_bool(client_t * client, const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	test_request_t request = TEST_REQUEST_INIT_ESC(test_case->code);
	if (0 != test_request_set_value_bool(&request)) {
		test_log_err("Test: failed to set value of request in test case [%s]\n", test_case->description);
		return -1;
	}
	test_request_set_random_n_bytes(&request);

	if (0 != client_send_request(client, &request)) {
		test_log_err("Test: failed to send Escape request in test case [%s]\n", test_case->description);
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}




/*
  TODO (acerion) 2024.02.11: implement properly: "cwdevice" may require some
  especial cases for values.
 */
static int test_fn_esc_cwdevice(client_t * client, __attribute__((unused)) const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	test_request_t request = TEST_REQUEST_INIT_ESC(CWDAEMON_ESC_REQUEST_CWDEVICE);
	if (0 != test_request_set_value_string(&request)) {
		test_log_err("Test: failed to set value of request in test case [%s]\n", test_case->description);
		return -1;
	}
	test_request_set_random_n_bytes(&request);

	if (0 != client_send_request(client, &request)) {
		test_log_err("Test: failed to send Escape request in test case [%s]\n", test_case->description);
		return -1;
	}

	test_sleep_nonintr(2);

	return 0;
}




/*
  TODO (acerion) 2024.02.11: implement properly: "sound system" may require
  some especial cases for values.
 */
static int test_fn_esc_sound_system(client_t * client, __attribute__((unused)) const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	test_request_t request = TEST_REQUEST_INIT_ESC(CWDAEMON_ESC_REQUEST_SOUND_SYSTEM);
	if (0 != test_request_set_value_string(&request)) {
		test_log_err("Test: failed to set value of request in test case [%s]\n", test_case->description);
		return -1;
	}
	test_request_set_random_n_bytes(&request);

	if (0 != client_send_request(client, &request)) {
		test_log_err("Test: failed to send Escape request in test case [%s]\n", test_case->description);
		return -1;
	}

	test_sleep_nonintr(2);

	return 0;
}




static int test_fn_esc_reply(client_t * client, __attribute__((unused)) const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	{
		test_request_t requested_reply = TEST_REQUEST_INIT_ESC(CWDAEMON_ESC_REQUEST_REPLY);
		if (0 != test_request_set_value_string(&requested_reply)) {
			test_log_err("Test: failed to set value of request in test case [%s]\n", test_case->description);
			return -1;
		}
		test_request_set_random_n_bytes(&requested_reply);

		if (0 != client_send_request(client, &requested_reply)) {
			test_log_err("Test: failed to send Escape request in test case [%s]\n", test_case->description);
			return -1;
		}
	}

	{
		test_request_t message = { 0 };
		if (0 != test_request_set_value_string(&message)) {
			test_log_err("Test: failed to set value of plain message in test case [%s]\n", test_case->description);
			return -1;
		}
		test_request_set_random_n_bytes(&message);

		if (0 != client_send_request(client, &message)) {
			test_log_err("Test: failed to send plain message in test case [%s]\n", test_case->description);
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
static int test_fn_esc_almost_all(client_t * client, __attribute__((unused)) const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	/* Don't use 'unsigned char' type for 'code'. With this range of values the
	   usage of 'unsigned char' would result in infinite loop. */
	for (unsigned int code = 0; code <= UCHAR_MAX; code++) {
		if ((char) code == CWDAEMON_ESC_REQUEST_EXIT) {
			/* Don't tell cwdaemon to exit in the middle of a test :) */
			continue;
		}

		/* Full message, including leading <ESC> and escape code. */
		test_request_t request = TEST_REQUEST_INIT_ESC(((char) code));
		if (0 != test_request_set_value_string(&request)) {
			test_log_err("Test: failed to set value of request for code 0x%02x in test case [%s]\n", code, test_case->description);
			return -1;
		}
		test_request_set_random_n_bytes(&request);

		if (0 != client_send_request(client, &request)) {
			test_log_err("Test: failed to send Escape request with code %u / 0x%02x in test case [%s]\n", code, (unsigned char) code, test_case->description);
			return -1;
		}

		test_millisleep_nonintr(200);
	}

	return 0;
}




// TODO (acerion) 2024.02.25: implement properly: make sure that messages have varying lengths.
static int test_fn_plain_message(client_t * client, __attribute__((unused)) const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	test_request_t request = { 0 };
	if (0 != test_request_set_value_string(&request)) {
		test_log_err("Test: failed to set value of request in test case [%s]\n", test_case->description);
		return -1;
	}
	test_request_set_random_n_bytes(&request);

	if (0 != client_send_request(client, &request)) {
		test_log_err("Test: failed to send plain message in test case [%s]\n", test_case->description);
		return -1;
	}

	/* At 40 wpm it takes ~30 seconds to play string consisting of 254 'e' characters. */
	test_sleep_nonintr(40);

	return 0;
}




// TODO (acerion) 2024.02.25: implement properly: make sure that messages have varying lengths.
static int test_fn_caret_message(client_t * client, __attribute__((unused)) const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	test_request_t request = { 0 };
	if (0 != test_request_set_value_string(&request)) {
		test_log_err("Test: failed to set value of request in test case [%s]\n", test_case->description);
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

		const unsigned int lower = 0;
		unsigned int upper = (unsigned int) request.n_bytes - 1;
		if (upper) {
			/* Be sure to not select index of value[] that would point to
			   terminating NUL. */
			upper--;
		}
		unsigned int mark = 0;
		cwdaemon_random_uint(lower, upper, &mark);
		if (request.bytes[mark] != '\0') { /* Another check for NUL, just in case. */
			request.bytes[mark] = '^';
		}
	}
	test_request_set_random_n_bytes(&request);

	if (0 != client_send_request(client, &request)) {
		test_log_err("Test: failed to send caret message in test case [%s]\n", test_case->description);
		return -1;
	}

	/* At 40 wpm it takes ~30 seconds to play string consisting of 254 'e' characters. */
	test_sleep_nonintr(40);

	return 0;
}


