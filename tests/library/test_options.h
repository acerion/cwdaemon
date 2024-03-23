#ifndef CWDAEMON_TEST_LIB_TEST_OPTIONS_H
#define CWDAEMON_TEST_LIB_TEST_OPTIONS_H




#include <stdbool.h>
#include <stdint.h>

#include <libcw.h>

#include "supervisor.h"




typedef struct test_options_t {
	enum cw_audio_systems sound_system;
	uint32_t random_seed;
	supervisor_id_t supervisor_id;
	bool invoked_help;
} test_options_t;




int test_options_get(int argc, char * const * argv, test_options_t * test_opts);




#endif /* #ifndef CWDAEMON_TEST_LIB_TEST_OPTIONS_H */

