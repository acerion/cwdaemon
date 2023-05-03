/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
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




#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "utils.h"




int build_full_device_path(char * path, size_t size, const char * input)
{
	if (!path) {
		return -EINVAL;
	}
	if (!size) {
		return -EINVAL;
	}
	if (!input) {
		return -EINVAL;
	}

	const char * dir = "/dev/";

	if (0 == strncmp(input, dir, strlen(dir))) {
		int n = snprintf(path, size, "%s", input);
		if (n <= 0 || (size_t) n >= size) {
			/* FIXME 2023.05.02: enable after moving cwdaemon_debug_f* to log.c
				(currently linking problems prevent me from using functions from
				log.c in unit tests). */
			// log_message(LOG_ERR, "Specified name of device is too long: [%s]", input);
			return -ENAMETOOLONG;
		}
	} else {
		int n = snprintf(path, size, "%s%s", dir, input);
		if (n <= 0 || (size_t) n >= size) {
			/* FIXME 2023.05.02: enable after moving cwdaemon_debug_f* to log.c
				(currently linking problems prevent me from using functions from
				log.c in unit tests). */
			// log_message(LOG_ERR, "Specified path of device is too long: [%s]", input);
			return -ENAMETOOLONG;
		}
	}

	/* FIXME 2023.05.02: enable after moving cwdaemon_debug_f* to log.c
		(currently linking problems prevent me from using functions from
		log.c in unit tests). */
	/* FIXME 2023.05.02: this debug is called too early and is not printed to
		console. */
	// cwdaemon_debug(CWDAEMON_VERBOSITY_D, __func__, __LINE__, "Device path to try: [%s]", path);
	return 0;
}



