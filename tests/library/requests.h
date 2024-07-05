#ifndef CWDAEMON_TESTS_LIB_REQUESTS_H
#define CWDAEMON_TESTS_LIB_REQUESTS_H




#include "events.h"




// When generating a request's value at random, what should be probability of
// generating valid or empty or invalid or random-bytes value?
//
// Numbers put into this struct should say "I want X/100 probability of
// generating valid value, Y/100 probability of generating empty value" etc.
//
// Sum of all numbers in the struct MUST be equal 100.
//
// Some of numbers may be zero, then value of corresponding type will not be
// generated.
//
// @reviewed_on{2024.07.04}
typedef struct tests_value_generation_probabilities_t {
	// Probability (0-100%) of generating request with valid value.
	//
	// A valid value is a string representation of e.g. tone (frequency) in
	// range CW_FREQUENCY_MIN-CW_FREQUENCY_MAX.
	unsigned int valid;

	// Probability (0-100%) of generating request with empty value.
	//
	// Empty value is just a string with zero bytes (not even a terminating
	// NUL).
	unsigned int empty;

	// Probability (0-100%) of generating request with invalid value.
	//
	/// Invalid value is a string representation of e.g. tone (frequency)
	/// lower than CW_FREQUENCY_MIN or higher than CW_FREQUENCY_MAX.
	unsigned int invalid;

	// Probability (0-100%) of generating request with random bytes put into
	// value.
	//
	// Once in a while the random bytes may (due to randomness) look like a
	// valid value or an invalid value or like an empty string.
	unsigned int random_bytes;
} tests_value_generation_probabilities_t;




int tests_requests_build_request_esc_port(test_request_t * request, tests_value_generation_probabilities_t const * percentages);




#endif /* #ifndef CWDAEMON_TESTS_LIB_REQUESTS_H */

