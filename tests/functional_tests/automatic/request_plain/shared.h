#ifndef REQUEST_PLAIN_SHARED_H
#define REQUEST_PLAIN_SHARED_H




#include "tests/library/events.h"
#include "tests/library/test_options.h"




typedef struct test_case_t {
	char const * description;            ///< Tester-friendly description of test case.
	const test_request_t plain_request;  ///< Bytes to be sent to cwdaemon server in the plain MESSAGE request.
	const event_t expected[EVENTS_MAX];  ///< Events that we expect to happen in this test case.
} test_case_t;




/// @reviewed_on{2024.05.01}
int run_test_cases(test_case_t const * test_cases, size_t n_test_cases, test_options_t const * test_opts, char const * test_name);




#endif /* #infdef REQUEST_PLAIN_SHARED_H */

