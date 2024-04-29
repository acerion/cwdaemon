#ifndef FUNCTIONAL_TEST_CARET_SHARED_H
#define FUNCTIONAL_TEST_CARET_SHARED_H




#include <stdlib.h>

#include "tests/library/client.h"
#include "tests/library/test_options.h"




typedef struct test_case_t {
	const char * description;                            /**< Tester-friendly description of test case. */
	test_request_t caret_request;                        /**< Text to be sent to cwdaemon server in the MESSAGE request. Full message, so it SHOULD include caret. */
	const test_reply_data_t expected_reply;   /**< What is expected to be received through socket from cwdaemon server. Full reply, so it SHOULD include terminating "\r\n". */
	const char expected_morse[400];              /**< What is expected to be received by Morse code receiver (without ending space). */
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




#endif /* #ifndef FUNCTIONAL_TEST_CARET_SHARED_H */

