#ifndef CWDAEMON_TEST_ENV_H
#define CWDAEMON_TEST_ENV_H




#include <stdbool.h>




typedef enum {
	/* Confirm that linked libcw doesn't use signals internally. Signals
	   interrupt sleep functions used in tests. */
	test_env_libcw_without_signals        = 0x00000001,
} test_env_flags_t;




/**
   @brief Confirm that test environment meets some expectations

   @param[in] flags Flags indicating which expectations to check

   @return true if the test env meets the expectations
   @return false otherwise
*/
bool test_env_is_usable(test_env_flags_t flags);




#endif /* #ifndef CWDAEMON_TEST_ENV_H */

