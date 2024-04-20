#ifndef CWDAEMON_TESTS_LIB_TEST_OPTIONS_H
#define CWDAEMON_TESTS_LIB_TEST_OPTIONS_H




#include <stdbool.h>
#include <stdint.h>

#include <libcw.h>

#include "supervisor.h"




typedef struct test_options_t {
	/// @brief Sound system to be used by cwdaemon server.
	///
	/// Used when starting local instance of tested server.
	enum cw_audio_systems sound_system;

	/// @brief Seed for random number generator used by test programs.
	///
	/// Used to initialized the random number generator.
	uint32_t random_seed;

	/// @brief Type of program supervising a cwdaemon server.
	///
	/// Used for supervising local instance of tested cwdaemon server.
	supervisor_id_t supervisor_id;

	/// @brief Show help text of test program?
	bool invoked_help;
} test_options_t;




/// @brief Fill given @p test_opts variable
///
/// The @p test_opts variable is filled with values found in:
/// 1. shell env variables dedicated to cwdaemon tests,
/// 2. command line options passed to test program
///
/// Values from env variables are read first (if they are set at all). Then
/// the command line options are parsed, and values of the options may
/// overwrite values coming from env variables.
///
/// If "-h/--help" command line option was passed to program, this function
/// sets test_opts::invoked_help to true. In that case a help text was show
/// on console, and the test program program should exit with success.
///
/// @reviewed_on{2024.04.19}
///
/// @param[in] argc Test program's arguments count (main's "argc" argument)
/// @param[in] argv Test program's arguments vector (main's "argv" argument)
///
/// @return 0 on successful parsing of env variables and/or options (even if there were no variables and/or options)
/// @return -1 on failure
int test_options_get(int argc, char * const * argv, test_options_t * test_opts);




#endif /* #ifndef CWDAEMON_TESTS_LIB_TEST_OPTIONS_H */

