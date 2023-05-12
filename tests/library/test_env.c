#include <libcw.h>
#include <stdint.h>
#include <stdio.h>

#include "test_env.h"




bool test_env_is_usable(test_env_flags_t flags)
{
	if (flags & test_env_libcw_without_signals) {
			uint32_t v = (uint32_t) cw_version();
			uint32_t current = (v & 0xffff0000) >> 16U;
			uint32_t revision =  v & 0x0000ffff;
			if (current < 7) {
				fprintf(stderr, "[EE] libcw version %d.%d is too low\n", current, revision);
				return false;
			}
	}

	return true;
}



