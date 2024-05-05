/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/// @file
///
/// Wrappers around pthread functions and data structures.




#include <stdio.h>
#include <string.h>

#include "log.h"
#include "tests/library/sleep.h"
#include "thread.h"




int thread_start(thread_t * thread)
{
	int retv = pthread_create(&thread->thread_id, &thread->thread_attr, thread->thread_fn, thread->thread_fn_arg);
	if (0 != retv) {
		test_log_err("Test: %s: failed to create thread: %s\n", thread->name, strerror(retv));
		return -1;
	}

	/* Very naive method of checking if a thread has started correctly: wait
	   a bit and check thread's flag. */
	test_millisleep_nonintr(100);
	if (thread->status != thread_running) {
		test_log_err("Test: %s: thread has not started correctly\n", thread->name);
		return -1;
	}

	return 0;
}




int thread_dtor(thread_t * thread)
{
	if (thread->thread_fn) {
		/* Cleaning up a thread makes sense only if there is some thread function. */
		pthread_attr_destroy(&thread->thread_attr);
		thread->thread_fn = NULL;
		thread->thread_fn_arg = NULL;
		thread->status = thread_not_started;
	}

	return 0;
}




