#ifndef FUNCTIONAL_TEST_REQUEST_ESC_REPLY_SHARED_H
#define FUNCTIONAL_TEST_REQUEST_ESC_REPLY_SHARED_H




#include <stdlib.h>

#include "tests/library/client.h"
#include "tests/library/test_options.h"




typedef struct test_case_t {
	const char * description;                            /**< Tester-friendly description of test case. */

	socket_send_data_t esc_request;                      /**< What is being sent to cwdaemon server as "esc reply" request. */
	const socket_receive_data_t expected_socket_reply;   /**< What is expected to be received through socket from cwdaemon server. Full reply, so it SHOULD include terminating "\r\n". */

	socket_send_data_t plain_request;                    /**< Text to be sent to cwdaemon server in the plain request - to be keyed by cwdaemon. */
	const char expected_morse_receive[400];              /**< What is expected to be received by Morse code receiver (without ending space). */

	event_t expected_events[EVENTS_MAX];                 /**< Events that we expect to happen in this test case. */
} test_case_t;




/**
   @brief Top-level function for running test cases

   @param[in] test_cases Test cases to run
   @param[in] n_test_cases Count of test cases in @p test_cases

   @return 0 if test passed
   @return -1 otherwise
*/
int run_test_cases(const test_case_t * test_cases, size_t n_test_cases, const test_options_t * test_opts);




#endif /* #ifndef FUNCTIONAL_TEST_REQUEST_ESC_REPLY_SHARED_H */

