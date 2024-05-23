#ifndef FUNCTIONAL_TEST_CARET_SHARED_H
#define FUNCTIONAL_TEST_CARET_SHARED_H




#include <stdlib.h>

#include "tests/library/client.h"
#include "tests/library/test_options.h"




/// @brief Test case for caret requests
typedef struct test_case_t {
	char const * description;       ///< Tester-friendly description of test case.
	test_request_t caret_request;   ///< Caret request with text to be keyed by cwdaemon server. Full request, so it SHOULD include caret.
	event_t expected[EVENTS_MAX];   ///< Events that we expect to happen in this test case.
} test_case_t;




/**
   @brief Top-level function for running test cases

   @reviewed_on{2024.05.01}

   @param[in] test_cases Test cases to run
   @param[in] n_test_cases Count of test cases in @p test_cases
   @param[in] test_opts Testing options collected from command line options and from env variables
   @param[in] test_name Name of the test

   @return 0 if test passed
   @return -1 otherwise
*/
int run_test_cases(const test_case_t * test_cases, size_t n_test_cases, const test_options_t * test_opts, char const * test_name);




#endif /* #ifndef FUNCTIONAL_TEST_CARET_SHARED_H */

