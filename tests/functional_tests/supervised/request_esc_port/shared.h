#ifndef FUNCTIONAL_TEST_REQUEST_ESC_PORT_SHARED_H
#define FUNCTIONAL_TEST_REQUEST_ESC_PORT_SHARED_H




#include <stdlib.h>

#include "tests/library/client.h"
#include "tests/library/test_options.h"




// The test case includes REPLY Escape request that should be processed by
// cwdaemon and used as reply, and also a PLAIN request that should be keyed
// on cwdevice and played through sound system. The reason for having these
// two requests in the test case and use them during test is to ensure that
// cwdaemon can correctly process and react to PLAIN request and REPLY Escape
// request while also correctly processing PORT Escape request. In other
// words, without the REPLY Escape request and PLAIN request the test would
// be too simple.
//
// The test case DOES NOT contain port. Port to be used in specific test case
// cycle and the entire PORT Escape request are being picked and generated in
// function running a test case in a loop.
typedef struct test_case_t {
	char const * const description;              ///< Tester-friendly description of test case.
	const test_request_t reply_esc_request;      ///< What is being sent to cwdaemon server as REPLY Escape request.
	const test_request_t plain_request;          ///< Text to be sent to cwdaemon server in the plain request - to be keyed by cwdaemon using sound system.
	const event_t expected[EVENTS_MAX];          ///< Events that we expect to happen in this test case.
} test_case_t;




/// @brief Top-level function for running test cases
///
/// @param[in] test_cases Test cases to run
/// @param[in] n_test_cases Count of test cases in @p test_cases
/// @param[in] test_opts Testing options collected from command line options and from env variables
/// @param[in] test_name Name of the test
///
/// @return 0 if test passed
/// @return -1 otherwise
int run_test_cases(test_case_t const * test_cases, size_t n_test_cases, test_options_t const * test_opts, char const * test_name);




#endif /* #ifndef FUNCTIONAL_TEST_REQUEST_ESC_PORT_SHARED_H */

