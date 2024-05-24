#ifndef FUNCTIONAL_TEST_REQUEST_ESC_CWDEVICE_SHARED_H
#define FUNCTIONAL_TEST_REQUEST_ESC_CWDEVICE_SHARED_H




#include <stdlib.h>

#include "tests/library/client.h"
#include "tests/library/test_options.h"




// The test case doesn't include CWDEVICE Escape request - the request is
// being built inside of code running a test case.
//
// The test case does include REPLY Escape request that should be processed
// by cwdaemon and used as reply, and also a PLAIN request that should be
// keyed on cwdevice (if the cwdevice is not "null"). The reason for having
// these two requests in the test case and use them during test is to ensure
// that cwdaemon can correctly process and react to PLAIN request and REPLY
// Escape request while also correctly processing CWDEVICE Escape request. In
// other words, without the Escape request the test would be too simple.
typedef struct test_case_t {
	char const * description;                    ///< Tester-friendly description of test case.
	char const * (* get_cwdevice_name)(void);    ///< Function returning a name of cwdevice to be used in this test case.
	const test_request_t reply_esc_request;      ///< What is being sent to cwdaemon server as REPLY Escape request.
	const test_request_t plain_request;          ///< Text to be sent to cwdaemon server in the plain request - to be keyed by cwdaemon.
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




/// @brief Get cwdevice name for "null" cwdevice
///
/// Returned pointer is a pointer to a static buffer inside of the function.
char const * get_null_cwdevice_name(void);




/// @brief Get cwdevice name for an invalid cwdevice
///
/// Returned pointer is a pointer to a static buffer inside of the function.
char const * get_invalid_cwdevice_name(void);




/// @brief Get cwdevice name for some cwdevice other than the default cwdevice
///
/// The name returned by this function is a valid cwdevice, but is not the
/// default device used or observed by test.
///
/// Returned pointer is a pointer to a static buffer inside of the function.
char const * get_valid_non_default_cwdevice_name(void);




/// @brief Get cwdevice name for the default cwdevice used during tests
///
/// The name returned by this function is for cwdevice that was passed to the
/// test as the default cwdevice to be used by this test.
///
/// Returned pointer is a pointer to a static buffer inside of the function.
char const * get_test_default_cwdevice_name(void);




#endif /* #ifndef FUNCTIONAL_TEST_REQUEST_ESC_CWDEVICE_SHARED_H */

