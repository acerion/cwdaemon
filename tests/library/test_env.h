#ifndef CWDAEMON_TESTS_LIB_TESTING_ENV_H
#define CWDAEMON_TESTS_LIB_TESTING_ENV_H




#include <stdbool.h>




typedef enum {
	/* Confirm that linked libcw doesn't use signals internally. Signals
	   interrupt sleep functions used in tests. */
	testing_env_libcw_without_signals        = 0x00000001,
} testing_env_flags_t;




/**
   @brief Confirm that tests environment meets some expectations

   @reviewed_on{2024.04.19}

   @param[in] flags Flags indicating which expectations to check

   @return true if the test env meets the expectations
   @return false otherwise
*/
bool testing_env_is_usable(testing_env_flags_t flags);




#endif /* #ifndef CWDAEMON_TESTS_LIB_TESTING_ENV_H */

