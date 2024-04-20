/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2023 Kamil Ignacak <acerion@wp.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <libcw.h>

#include "log.h"
#include "test_env.h"




bool testing_env_is_usable(testing_env_flags_t flags)
{
	if (flags & testing_env_libcw_without_signals) {
		const uint32_t v = (uint32_t) cw_version();
		const uint32_t current = (v & 0xffff0000) >> 16U;
		const uint32_t revision =  v & 0x0000ffff;
		if (current < 7) {
			test_log_err("Test: libcw version %"PRIu32".%"PRIu32" is too low\n", current, revision);
			return false;
		}
	}

	return true;
}



