#ifndef CWDAEMON_TESTS_LIB_TESTING_ENV_H
#define CWDAEMON_TESTS_LIB_TESTING_ENV_H




#include <stdbool.h>




typedef enum {
	/* Confirm that linked libcw doesn't use signals internally. Signals
	   interrupt sleep functions used in tests. */
	testing_env_libcw_without_signals        = 0x00000001,

	/// @brief A real cwdevice is present on test machine
	///
	/// The cwdevice that was configured at compile time and that should be
	/// used as the default cwdevice during tests is present on test machine,
	/// in /dev/ directory. The device is "real", i.e. it has a keying pin.
	testing_env_real_cwdevice_is_present     = 0x00000002,
} testing_env_flags_t;




/**
   @brief Confirm that tests environment meets some expectations

   @param[in] flags Flags indicating which expectations to check

   @return true if the test env meets the expectations
   @return false otherwise
*/
bool testing_env_is_usable(testing_env_flags_t flags);




#endif /* #ifndef CWDAEMON_TESTS_LIB_TESTING_ENV_H */

