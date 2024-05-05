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




/// @file
///
/// Logging functions for tests.




#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "tests/library/log.h"
#include "tests/library/test_defines.h"




// Large enough to fit largest request sent to cwdaemon.
#define LOG_BUF_SIZE (7 * CLIENT_SEND_BUFFER_SIZE)




// @reviewed_on{2024.05.04}
void test_log_persistent(int priority, const char * format, ...)
{
	const char * prio_str = NULL;
	switch (priority) {
	case LOG_ERR:
		prio_str = "EE";
		break;
	case LOG_WARNING:
		prio_str = "WW";
		break;
	case LOG_NOTICE:
		prio_str = "NN";
		break;
	case LOG_INFO:
		prio_str = "II";
		break;
	case LOG_DEBUG:
		prio_str = "DD";
		break;
	default:
		prio_str = "??";
		break;
	}

	// File in /tmp should be good enough for now.
	FILE * file = fopen("/tmp/cwdaemon_tests.log", "a");
	if (!file) {
		test_log_err("Test logging: can't open log file: %s\n", strerror(errno));
		return;
	}

	va_list ap;
	char buf[LOG_BUF_SIZE] = { 0 };
	va_start(ap, format);
	vsnprintf(buf, sizeof (buf), format, ap);
	va_end(ap);

	char timestamp[sizeof ("'Sat May  4 12:20:37 2024' and some spare space")] = { 0 };
	time_t const now = time(NULL);
	ctime_r(&now, timestamp);
	const size_t len = strlen(timestamp);
	if (len > 0 && timestamp[len - 1] == '\n') {
		timestamp[len - 1] = '\0';
	}

	fprintf(file, "[%s] [%s] %s", prio_str, timestamp, buf);

	fclose(file);

	return;
}

