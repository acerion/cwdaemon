/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2024 Kamil Ignacak <acerion@wp.pl>
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




#include <stdio.h>
#include <string.h>

#include "cwdevice.h"
#include "log.h"




/// @file
///
/// Functions related to cwdevice, but not directly to observing of the
/// cwdevice.




// @reviewed_on{2024.05.25}
char * cwdevice_get_full_path(char const * name, char * path, size_t size)
{
	if (0 == strcmp("null", name)) {
		// In the context of cwdaemon this is a special device, so it gets a
		// special treatment.
		snprintf(path, size, "%s", name);
		return path;
	}

	char const * const dev_dir = "/dev/";
	if (0 == strncmp(name, dev_dir, strlen(dev_dir))) {
		snprintf(path, size, "%s", name);
	} else {
		snprintf(path, size, "%s%s", dev_dir, name);
	}

	return path;
}


