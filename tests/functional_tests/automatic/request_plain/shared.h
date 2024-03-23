#ifndef REQUEST_PLAIN_SHARED_H
#define REQUEST_PLAIN_SHARED_H




#include "tests/library/client.h"
#include "tests/library/test_options.h"




typedef struct test_case_t {
	const char * description;             /**< Tester-friendly description of test case. */
	socket_send_data_t plain_request;     /**< Bytes to be sent to cwdaemon server in the plain MESSAGE request. */
	const char * expected_morse_receive;  /**< What is expected to be received by Morse code receiver (without ending space). */
	event_t expected_events[EVENTS_MAX];  /**< Events that we expect to happen in this test case. */
} test_case_t;




int run_test_cases(test_case_t * test_cases, size_t n_test_cases, const test_options_t * test_opts);




#endif /* #infdef REQUEST_PLAIN_SHARED_H */

