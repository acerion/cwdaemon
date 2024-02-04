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
 * GNU Library General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




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
				test_log_err("Test: libcw version %d.%d is too low\n", current, revision);
				return false;
			}
	}

	return true;
}



