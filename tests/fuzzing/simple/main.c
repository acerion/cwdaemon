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

static int test_fn_plain_request(client_t * client, const test_case_t * test_case, morse_receiver_t * morse_receiver);
static int test_fn_caret_request(client_t * client, const test_case_t * test_case, morse_receiver_t * morse_receiver);

static int test_request_set_value_string(test_request_t * request);
static int test_request_set_value_bool(test_request_t * request);
static int test_request_set_value_int(test_request_t * request, int range_low, int range_high);

static int get_random_val_n_bytes(size_t max_val_n_bytes, size_t * random_val_n_bytes);

static char const * g_test_name = "fuzzing - simple";




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
// @reviewed_on{2024.05.04}
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
	{ .description = "esc request - almost all",      .fn = test_fn_esc_almost_all  },
#endif
#if 1
	{ .description = "plain request",                 .fn = test_fn_plain_request   },
	{ .description = "plain request",                 .fn = test_fn_plain_request   },
	{ .description = "plain request",                 .fn = test_fn_plain_request   },
#endif
#if 1
	{ .description = "caret request",                 .fn = test_fn_caret_request   },
	{ .description = "caret request",                 .fn = test_fn_caret_request   },
	{ .description = "caret request",                 .fn = test_fn_caret_request   },
#endif
};




static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_options_t * test_opts);
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver);
static int test_run(const test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver);




// @reviewed_on{2024.05.04}
int main(int argc, char * const * argv)
{
	if (!testing_env_is_usable(testing_env_libcw_without_signals)) {
		test_log_err("Test: preconditions for testing env are not met, exiting test [%s]\n", g_test_name);
		exit(EXIT_FAILURE);
	}

	test_options_t test_opts = { .sound_system = CW_AUDIO_NULL, .supervisor_id = supervisor_id_valgrind };
	if (0 != test_options_get(argc, argv, &test_opts)) {
		test_log_err("Test: failed to process command line options for test [%s]\n", g_test_name);
		exit(EXIT_FAILURE);
	}
	if (test_opts.invoked_help) {
		/* Help text was printed as requested. Now exit. */
		exit(EXIT_SUCCESS);
	}

	const uint32_t seed = cwdaemon_srandom(test_opts.random_seed);
	test_log_debug("Test: random seed: 0x%08x (%u)\n", seed, seed);
	// There may be a lot of strange chars printed to console during this
	// test. Some of them may erase initial logs. Better save the info about
	// seed to some safe place.
	test_log_persistent(LOG_INFO, "Test [%s] random seed: 0x%08x (%u)\n", g_test_name, seed, seed);

	bool failure = false;
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
	server_t server = { .events = &events };
	client_t client = { .events = &events };
	morse_receiver_t morse_receiver = { .events = &events };

	if (0 != test_setup(&server, &client, &morse_receiver, &test_opts)) {
		test_log_err("Test: failed at test setup for test [%s]\n", g_test_name);
		failure = true;
		goto cleanup;
	}

	if (test_run(g_test_cases, n_test_cases, &client, &morse_receiver)) {
		test_log_err("Test: failed at running test cases for test [%s]\n", g_test_name);
		failure = true;
		goto cleanup;
	}

 cleanup:
	if (0 != test_teardown(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at test tear down for test [%s]\n", g_test_name);
		failure = true;
	}

	test_log_newline(); /* Visual separator. */
	if (failure) {
		test_log_err("Test: FAIL ([%s] test)\n", g_test_name);
		test_log_newline(); /* Visual separator. */
		exit(EXIT_FAILURE);
	}
	test_log_info("Test: PASS ([%s] test)\n", g_test_name);
	test_log_newline(); /* Visual separator. */
	exit(EXIT_SUCCESS);
}




/// @brief Prepare resources used to execute set of test cases
///
/// @reviewed_on{2024.05.04}
///
/// @return 0 on success
/// @return -1 on failure
static int test_setup(server_t * server, client_t * client, morse_receiver_t * morse_receiver, const test_options_t * test_opts)
{
	const int wpm = 40;

	/* Prepare local test instance of cwdaemon server. */
	const server_options_t server_opts = {
		.tone           = tests_get_test_tone(),
		.sound_system   = test_opts->sound_system,
		.cwdevice_name  = TESTS_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
		.supervisor_id =  test_opts->supervisor_id,
	};
	if (0 != server_start(&server_opts, server)) {
		test_log_err("Test: tailed to start cwdaemon server %s\n", "");
		return -1;
	}


	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.01.27: remove casting. */
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		return -1;
	}
	client_socket_receive_enable(client);
	if (0 != client_socket_receive_start(client)) {
		test_log_err("Test: failed to start socket receiver %s\n", "");
		return -1;
	}


	const morse_receiver_config_t morse_config = { .wpm = wpm };
	if (0 != morse_receiver_configure(&morse_config, morse_receiver)) {
		test_log_err("Test: failed to configure Morse receiver %s\n", "");
		return -1;
	}


	return 0;
}




/// @brief Clean up resources used to execute set of test cases
///
/// @reviewed_on{2024.05.04}
///
/// @return 0 on success
/// @return -1 on failure
static int test_teardown(server_t * server, client_t * client, morse_receiver_t * morse_receiver)
{
	bool failure = false;

	/*
	  Terminate local test instance of cwdaemon server. Always do it first
	  since the server is the main trigger of events in the test.

	  The third arg to the 'stop' function is 'true' because we want to fuzz
	  the daemon till the very end. Unfortunately we can send EXIT Escape
	  request only once per test run :(

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

	morse_receiver_deconfigure(morse_receiver);

	client_socket_receive_stop(client);
	client_disconnect(client);
	client_dtor(client);

	return failure ? -1 : 0;
}




/// @brief Run test cases. Evaluate results (the events) of each test case.
///
/// In contrast to other tests of cwdaemon, this "test_run()" function
/// selects tests cases at random. This randomness may help me in triggering
/// incorrect states in cwdaemon server.
///
/// @reviewed_on{2024.05.04}
///
/// @return 0 on success (no test case has failed)
/// @return -1 otherwise
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




/// @brief Set of values describing how a value put into request will be generated
///
/// TODO (acerion) 2024.05.04: we could really benefit from "out of range"
/// enum value. It would be useful for integer values of requests (e.g. wpm
/// smaller than MIN or larger than MAX), and maybe also for string values of
/// requests (e.g. a random string of printable characters for CWDEVICE
/// Escape request).
typedef enum value_generation_mode_t {
	value_generation_mode_nul_array,      /* Return array of bytes that can be interpreted as empty C string with zero or more NUL bytes. */
	value_generation_mode_in_range,       /* Return array of bytes that can be interpreted as string representation of a random value that is in range of valid values. */
	value_generation_mode_random_bytes,   /* Return array of completely random bytes. */
} value_generation_mode_t;




/// @brief Return a random value of variable of value_generation_mode_t type
///
/// @reviewed_on{2024.05.04}
///
/// @param[out] value_mode Mode of generating a value
///
/// @return 0 on success
/// @return -1 on failure
static int get_value_generation_mode(value_generation_mode_t * value_mode)
{
	const unsigned int lower = 0;
	const unsigned int upper = 12;
	unsigned int x = 0;
	if (0 != cwdaemon_random_uint(lower, upper, &x)) {
		test_log_err("Test: value generation mode = UNKNOWN (failed to get random value): %u\n", *value_mode);
		return -1;
	}

	// Here we control how frequently some of modes are selected. 'empty' is
	// least likely of all modes.
	if (x <= 1) {
		*value_mode = value_generation_mode_nul_array;
		test_log_info("Test: value generation mode = NUL ARRAY %s\n", "");
	} else if (x <= 6) {
		*value_mode = value_generation_mode_in_range;
		test_log_info("Test: value generation mode = IN RANGE %s\n", "");
	} else if (x <= 12) {
		*value_mode = value_generation_mode_random_bytes;
		test_log_info("Test: value generation mode = RANDOM BYTES %s\n", "");
	} else {
		test_log_err("Test: value generation mode = UNKNOWN: %u\n", x);
		return -1;
	}

	return 0;
}




/**
   @brief Pick a count of bytes for value of a request

   If you want to randomize count of bytes for value of a @p request, do it
   with this function.

   The function slightly prioritizes byte counts that are close to size of
   cwdaemon's receive buffer. The reason for this is: I would like this test
   to be able to test off-by-one errors and buffer overflows in cwdaemon's
   code handling requests and replies.

   @reviewed_on{2024.05.04}

   @param[in] max_val_n_bytes Maximum acceptable count of bytes for value to be returned
   @param[out] random_val_n_bytes Random generated count of bytes for value

   @return 0 on success
   @return -1 on failure
*/
static int get_random_val_n_bytes(size_t max_val_n_bytes, size_t * random_val_n_bytes)
{
	// Decide how do we want to pick the value of val_n_bytes.
	unsigned int const mode_min = 0;
	unsigned int const mode_max = 10;
	unsigned int mode = 0;;
	if (0 != cwdaemon_random_uint(mode_min, mode_max, &mode)) {
		test_log_err("Failed to generate random uint for mode in %s\n", __func__);
		return -1;
	}

	// Some of these modes are less likely and other are more likely to be
	// selected.
	if (mode <= 1) {
		// Use max_val_n_bytes.
		*random_val_n_bytes = max_val_n_bytes;
		return 0;

	} else if (mode <= 5) {
		// Generate random value no larger than max_val_n_bytes.
		const unsigned int lower = 0;
		const unsigned int upper = (unsigned int) max_val_n_bytes;
		unsigned int val = 0;
		if (0 != cwdaemon_random_uint(lower, upper, &val)) {
			test_log_err("Failed to generate random uint for mode 1 in %s\n", __func__);
			return -1;
		}
		*random_val_n_bytes = val;
		return 0;

	} else if (mode <= mode_max) {
		// Generate random value close to cwdaemon's receive buffer size.
		// This allows us to put more focus on testing for boundary problems
		// and buffer overflows in server.
		//
		// TODO (acerion) 2024.05.04 Make sure that 'upper' is lower than
		// 'max_val_n_bytes' arg,
		const unsigned int lower = CWDAEMON_REQUEST_SIZE_MAX - 5;
		const unsigned int upper = CWDAEMON_REQUEST_SIZE_MAX + 35;
		unsigned int val = 0;
		if (0 != cwdaemon_random_uint(lower, upper, &val)) {
			test_log_err("Failed to generate random uint for mode 2 in %s\n", __func__);
			return -1;
		}
		*random_val_n_bytes = val;
		return 0;

	} else {
		test_log_err("Test: unsupported mode %u in %s\n", mode, __func__);
		return -1;
	}
}




/// @brief Set value of request for requests that should contain a string representation of boolean parameter
///
/// Some requests (e.g. PTT_STATE Escape request) should contain a string
/// representation of a boolean. This function sets such value in given @p
/// request.
///
/// Count of bytes/characters in the value is randomized.
///
/// @reviewed_on{2024.05.04}
///
/// @return 0 on success
/// @return -1 on error
static int test_request_set_value_bool(test_request_t * request)
{
	bool value_in_range = false;

	// Skip potential header of Escape request (the Escape character + Escape
	// code) that may have been already added by caller. We don't want to
	// overwrite it here.
	char * const val_start = request->bytes + request->n_bytes;
	size_t val_n_bytes = sizeof (request->bytes) - request->n_bytes;
	val_n_bytes--; // Make space for appending a (potential) terminating NUL.

	// Randomize count of bytes for value.
	size_t random_val_n_bytes = 0;
	if (0 != get_random_val_n_bytes(val_n_bytes, &random_val_n_bytes)) {
		test_log_err("Test: failed to get random value size in %s\n", __func__);
		return -1;
	}
	val_n_bytes = random_val_n_bytes;

	// Generate the value with specified count of bytes.
	value_generation_mode_t value_mode = value_generation_mode_random_bytes;
	get_value_generation_mode(&value_mode);
	switch (value_mode) {
	case value_generation_mode_nul_array:
		memset(val_start, '\0', val_n_bytes);
		break;
	case value_generation_mode_in_range:
		if (0 != cwdaemon_random_bool(&value_in_range)) {
			test_log_err("Test: failed to get random in-range boolean value in %s\n", __func__);
			return -1;
		}
		snprintf(val_start, val_n_bytes, "%u", (unsigned int) value_in_range);
		// Notice that this overwrites a random count of bytes. We just
		// generate a nice "in range" representation of value, with count of
		// bytes decided by size of the value.
		val_n_bytes = strlen(val_start);
		break;
	case value_generation_mode_random_bytes:
		if (0 != cwdaemon_random_bytes(val_start, val_n_bytes)) {
			test_log_err("Test: failed to get random bytes in %s\n", __func__);
			return -1;
		}
		break;
	default:
		test_log_err("Test: unhandled value generation mode %u in %s\n", value_mode, __func__);
		return -1;
	}

	// Maybe append a NUL. cwdaemon should be able to handle strings in
	// requests with and without terminating NUL.
	bool const append_nul = random_val_n_bytes % 2;
	if (append_nul) {
		val_start[val_n_bytes++] = '\0';
	}

	request->n_bytes += val_n_bytes;

	// Debug. Notice that we only print val_n_bytes bytes of value - without
	// potential Escape byte and Escape code.
#if 1
	char printable[PRINTABLE_BUFFER_SIZE(sizeof (request->bytes))] = { 0 };
	get_printable_string(val_start, val_n_bytes, printable, sizeof (printable));
	test_log_debug("Generated %zu bytes of bool value: [%s]\n", val_n_bytes, printable);
#endif

	return 0;
}




/// @brief Set value of request for requests that should contain a string representation of integer parameter
///
/// Some requests (e.g. SPEED or TONE Escape request) should contain a string
/// representation of an integer. This function sets such value in given @p
/// request.
///
/// Range of valid values of the integer parameter is specified by caller
/// through @p range_low and @p range_high (inclusive).
///
/// Count of bytes/characters in the value is randomized.
///
/// @reviewed_on{2024.05.04}
///
/// @return 0 on success
/// @return -1 on error
static int test_request_set_value_int(test_request_t * request, int range_low, int range_high)
{
	unsigned int value_in_range = 0;

	// Skip potential header of Escape request (the Escape character + Escape
	// code) that may have been already added by caller. We don't want to
	// overwrite it here.
	char * const val_start = request->bytes + request->n_bytes;
	size_t val_n_bytes = sizeof (request->bytes) - request->n_bytes;
	val_n_bytes--; // Make space for appending a (potential) terminating NUL.

	// Randomize count of bytes for value.
	size_t random_val_n_bytes = 0;
	if (0 != get_random_val_n_bytes(val_n_bytes, &random_val_n_bytes)) {
		test_log_err("Test: failed to get random value size in %s\n", __func__);
		return -1;
	}
	val_n_bytes = random_val_n_bytes;

	// Generate the value with specified count of bytes.
	value_generation_mode_t value_mode = value_generation_mode_random_bytes;
	get_value_generation_mode(&value_mode);
	switch (value_mode) {
	case value_generation_mode_nul_array:
		memset(val_start, '\0', val_n_bytes);
		break;
	case value_generation_mode_in_range:
		// TODO (acerion) 2024.04.19: cwdaemon's weighting option accepts
		// also negative values, so this call to _uint() doesn't cover all
		// range. Use _int() function?
		if (0 != cwdaemon_random_uint((unsigned int) range_low, (unsigned int) range_high, &value_in_range)) {
			test_log_err("Test: failed to get random in-range integer value in %s\n", __func__);
			return -1;
		}
		snprintf(val_start, val_n_bytes, "%u", value_in_range);
		// Notice that this overwrites a random count of bytes. We just
		// generate a nice "in range" representation of value, with count of
		// bytes decided by size of the value.
		val_n_bytes = strlen(val_start);
		break;
	case value_generation_mode_random_bytes:
		if (0 != cwdaemon_random_bytes(val_start, val_n_bytes)) {
			test_log_err("Test: failed to get random bytes in %s\n", __func__);
			return -1;
		}
		break;
	default:
		test_log_err("Test: unhandled value generation mode %u in %s\n", value_mode, __func__);
		return -1;
	}

	// Maybe append a NUL. cwdaemon should be able to handle strings in
	// requests with and without terminating NUL.
	bool const append_nul = random_val_n_bytes % 2;
	if (append_nul) {
		val_start[val_n_bytes++] = '\0';
	}

	request->n_bytes += val_n_bytes;

	// Debug. Notice that we only print val_n_bytes bytes of value - without
	// potential Escape byte and Escape code.
#if 1
	char printable[PRINTABLE_BUFFER_SIZE(sizeof (request->bytes))] = { 0 };
	get_printable_string(val_start, val_n_bytes, printable, sizeof (printable));
	test_log_debug("Generated %zu bytes of int value: [%s]\n", val_n_bytes, printable);
#endif

	return 0;
}




/// @brief Set value of request for requests that should contain a string value
///
/// Some requests (e.g. PLAIN request or REPLY Escape request) should contain
/// a string of characters that should be e.g. played by cwdaemon server.
/// This function sets such value in given @p request.
///
/// Count of bytes/characters in the value is randomized.
///
/// @reviewed_on{2024.05.04}
///
/// @return 0 on success
/// @return -1 on error
static int test_request_set_value_string(test_request_t * request)
{
	// Skip potential header of Escape request (the Escape character + Escape
	// code) that may have been already added by caller. We don't want to
	// overwrite it here. */
	char * const val_start = request->bytes + request->n_bytes;
	size_t val_n_bytes = sizeof (request->bytes) - request->n_bytes;
	val_n_bytes--; // Make space for appending a (potential) terminating NUL.

	// Randomize count of bytes for value.
	size_t random_val_n_bytes = 0;
	if (0 != get_random_val_n_bytes(val_n_bytes, &random_val_n_bytes)) {
		test_log_err("Test: failed to get random value size in %s\n", __func__);
		return -1;
	}
	val_n_bytes = random_val_n_bytes;

	// Generate the value with specified count of bytes.
	value_generation_mode_t value_mode = value_generation_mode_random_bytes;
	get_value_generation_mode(&value_mode);
	switch (value_mode) {
	case value_generation_mode_nul_array:
		memset(val_start, '\0', val_n_bytes);
		break;
	case value_generation_mode_in_range:
		// "in range" is interpreted here as "string of printable
		// characters".
		if (0 != cwdaemon_random_printable_string(val_start, val_n_bytes)) {
			test_log_err("Test: failed to get random printable characters in %s\n", __func__);
			return -1;
		}
		break;
	case value_generation_mode_random_bytes:
		if (0 != cwdaemon_random_bytes(val_start, val_n_bytes)) {
			test_log_err("Test: failed to get random bytes in %s\n", __func__);
			return -1;
		}
		break;
	default:
		test_log_err("Test: unhandled value generation mode %u in %s\n", value_mode, __func__);
		return -1;
	}

	// Maybe append a NUL. cwdaemon should be able to handle strings in
	// requests with and without terminating NUL.
	bool const append_nul = random_val_n_bytes % 2;
	if (append_nul) {
		val_start[val_n_bytes++] = '\0';
	}

	request->n_bytes += val_n_bytes;

	// Debug. Notice that we only print val_n_bytes bytes of value - without
	// potential Escape byte and Escape code.
#if 1
	char printable[PRINTABLE_BUFFER_SIZE(sizeof (request->bytes))] = { 0 };
	get_printable_string(val_start, val_n_bytes, printable, sizeof (printable));
	test_log_debug("Generated %zu bytes of string value: [%s]\n", val_n_bytes, printable);
#endif

	return 0;
}




/**
   @brief Send Escape request that doesn't require a value

   This Escape request doesn't require a value, but we will try to include
   the value in the request anyway to stress-test cwdaemon. The function will
   just get a string of bytes and use it as value.

   Escape request code will be taken from @p test_case.

   @reviewed_on{2024.05.04}

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

   @reviewed_on{2024.05.04}

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

   @reviewed_on{2024.05.04}

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

	if (0 != client_send_request(client, &request)) {
		test_log_err("Test: failed to send Escape request in test case [%s]\n", test_case->description);
		return -1;
	}

	test_sleep_nonintr(1);

	return 0;
}




/// @brief Send CWDEVICE Escape request
///
/// TODO (acerion) 2024.02.11: implement properly: "cwdevice" may require
/// some especial cases for values, e.g. a list of valid device paths.
///
/// @reviewed_on{2024.05.04}
///
/// @param client cwdaemon client to use for sending of the request
/// @param[in] test_case Current test case
/// @param morse_receiver Morse receiver that may or may not be used to receive Morse code on cwdevice during additional verification
///
/// @return 0 on success
/// @return -1 on failure
static int test_fn_esc_cwdevice(client_t * client, __attribute__((unused)) const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	test_request_t request = TEST_REQUEST_INIT_ESC(CWDAEMON_ESC_REQUEST_CWDEVICE);
	if (0 != test_request_set_value_string(&request)) {
		test_log_err("Test: failed to set value of request in test case [%s]\n", test_case->description);
		return -1;
	}

	if (0 != client_send_request(client, &request)) {
		test_log_err("Test: failed to send Escape request in test case [%s]\n", test_case->description);
		return -1;
	}

	test_sleep_nonintr(2);

	return 0;
}





/// @brief Send SOUND_SYSTEM Escape request
///
/// TODO (acerion) 2024.02.11: implement properly: "sound system" may require
/// some especial cases for values, e.g. a list of valid sound systems.
///
/// @reviewed_on{2024.05.04}
///
/// @param client cwdaemon client to use for sending of the request
/// @param[in] test_case Current test case
/// @param morse_receiver Morse receiver that may or may not be used to receive Morse code on cwdevice during additional verification
///
/// @return 0 on success
/// @return -1 on failure
static int test_fn_esc_sound_system(client_t * client, __attribute__((unused)) const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	test_request_t request = TEST_REQUEST_INIT_ESC(CWDAEMON_ESC_REQUEST_SOUND_SYSTEM);
	if (0 != test_request_set_value_string(&request)) {
		test_log_err("Test: failed to set value of request in test case [%s]\n", test_case->description);
		return -1;
	}

	if (0 != client_send_request(client, &request)) {
		test_log_err("Test: failed to send Escape request in test case [%s]\n", test_case->description);
		return -1;
	}

	test_sleep_nonintr(2);

	return 0;
}




/// @brief Send REPLY Escape request followed by PLAIN request
///
/// @reviewed_on{2024.05.04}
///
/// @param client cwdaemon client to use for sending of the request
/// @param[in] test_case Current test case
/// @param morse_receiver Morse receiver that may or may not be used to receive Morse code on cwdevice during additional verification
///
/// @return 0 on success
/// @return -1 on failure
static int test_fn_esc_reply(client_t * client, __attribute__((unused)) const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	{
		test_request_t reply_request = TEST_REQUEST_INIT_ESC(CWDAEMON_ESC_REQUEST_REPLY);
		if (0 != test_request_set_value_string(&reply_request)) {
			test_log_err("Test: failed to set value of REPLY Escape request in test case [%s]\n", test_case->description);
			return -1;
		}

		if (0 != client_send_request(client, &reply_request)) {
			test_log_err("Test: failed to send REPLY Escape request in test case [%s]\n", test_case->description);
			return -1;
		}
	}

	{
		test_request_t plain_request = { 0 };
		if (0 != test_request_set_value_string(&plain_request)) {
			test_log_err("Test: failed to set value of PLAIN request in test case [%s]\n", test_case->description);
			return -1;
		}

		if (0 != client_send_request(client, &plain_request)) {
			test_log_err("Test: failed to send PLAIN request in test case [%s]\n", test_case->description);
			return -1;
		}

		// At 40 wpm it takes ~30 seconds to play string consisting of 254
		// 'e' characters.
		//
		// TODO (acerion) 2024.05.04: replace this with receiver receiving a
		// message on cwdevice.
		test_sleep_nonintr(40);
	}

	/* TODO acerion 2024.03.01: add here receiving of reply. */

	return 0;
}




/**
   @brief Send Escape requests with (almost) all values of Escape codes

   Function sends Escape requests that contain code values from full range of
   'char' type. The function is meant to test behaviour of cwdaaemon when
   unsupported Escape requests are being sent.

   Of course I could wait for some other "random" functions to generate all
   possible (valid and invalid) Escape requests, but that would take
   bazillion of iterations. Instead of that I'm using this function that is
   designed to test just that one aspect.

   Each call of the function sends 255 Escape requests with 255 values of
   Escape code - with exception of EXIT Escape request.

   EXIT Escape request is not sent because I don't want to terminate the test
   prematurely. EXIT Escape request is tested by setting third arg of
   local_server_stop_fuzz() to 'true' in this file.

   TODO acerion 2024.05.04: consider writing a separate test that focuses
   only on fuzzing EXIT Escape request.

   @reviewed_on{2024.05.04}

   @return 0 on success
   @return -1 on failure
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

		// Prepare full request, including leading Escape character and
		// Escape request code.
		test_request_t request = TEST_REQUEST_INIT_ESC(((char) code));
		if (0 != test_request_set_value_string(&request)) {
			test_log_err("Test: failed to set value of request for code 0x%02x in test case [%s]\n", code, test_case->description);
			return -1;
		}

		if (0 != client_send_request(client, &request)) {
			test_log_err("Test: failed to send Escape request with code %u / 0x%02x in test case [%s]\n", code, (unsigned char) code, test_case->description);
			return -1;
		}

		test_millisleep_nonintr(200);
	}

	return 0;
}




/// @brief Send PLAIN request
///
/// @reviewed_on{2024.05.04}
///
/// @param client cwdaemon client to use for sending of the request
/// @param[in] test_case Current test case
/// @param morse_receiver Morse receiver that may or may not be used to receive Morse code on cwdevice during additional verification
///
/// @return 0 on success
/// @return -1 on failure
static int test_fn_plain_request(client_t * client, __attribute__((unused)) const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	test_request_t request = { 0 };
	if (0 != test_request_set_value_string(&request)) {
		test_log_err("Test: failed to set value of PLAIN request in test case [%s]\n", test_case->description);
		return -1;
	}

	if (0 != client_send_request(client, &request)) {
		test_log_err("Test: failed to send PLAIN request in test case [%s]\n", test_case->description);
		return -1;
	}

	// At 40 wpm it takes ~30 seconds to play string consisting of 254 'e'
	// characters.
	//
	// TODO (acerion) 2024.05.04: use receiver to wait for end of keying.
	test_sleep_nonintr(40);

	return 0;
}




/// @brief Send CARET request
///
/// @reviewed_on{2024.05.04}
///
/// @param client cwdaemon client to use for sending of the request
/// @param[in] test_case Current test case
/// @param morse_receiver Morse receiver that may or may not be used to receive Morse code on cwdevice during additional verification
///
/// @return 0 on success
/// @return -1 on failure
static int test_fn_caret_request(client_t * client, __attribute__((unused)) const test_case_t * test_case, __attribute__((unused)) morse_receiver_t * morse_receiver)
{
	test_request_t request = { 0 };
	if (0 != test_request_set_value_string(&request)) {
		test_log_err("Test: failed to set value of CARET request in test case [%s]\n", test_case->description);
		return -1;
	}

	// Add up to 3 carets. Randomize the position of carets. Don't insert NUL
	// after the first caret, see how server handles characters after the
	// first caret.
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

		// Insert a caret at random position, but rather in a second part of
		// value string.
		//
		// test_request_t::bytes is an array of bytes that may or may not be
		// terminated with NUL. So we don't have to care too much about
		// overwriting terminating NUL, because:
		//  - the NUL may not be there in the first place,
		//  - client sends test_request_t::n_bytes to server. It doesn't care
		//    about presence or absence of terminating NUL in
		//    test_request_t::bytes.
		// We should be good as long as we don't try to write beyond
		// test_request_t::n_bytes.
		unsigned int const upper = (unsigned int) request.n_bytes - 1;
		unsigned int const lower = upper / 2;
		unsigned int mark = 0;
		if (0 != cwdaemon_random_uint(lower, upper, &mark)) {
			// Protect us from always selecting '0' as a fallback position
			// for caret upon errors of random().
			test_log_err("Test: failed to generate position of a caret in test case [%s]\n", test_case->description);
			return -1;
		}
		test_log_debug("Test: caret will be placed at position %u\n", mark);
		request.bytes[mark] = '^';
	}

	if (0 != client_send_request(client, &request)) {
		test_log_err("Test: failed to send caret message in test case [%s]\n", test_case->description);
		return -1;
	}

	// At 40 wpm it takes ~30 seconds to play string consisting of 254 'e'
	// characters.
	///
	// TODO (acerion) 2024.05.04: use receiver to wait for end of keying.
	test_sleep_nonintr(40);

	return 0;
}


