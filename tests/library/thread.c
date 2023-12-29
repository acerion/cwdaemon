/*
 * thread.c - thread functions for cwdaemon tests
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




#include <stdio.h>
#include <string.h>

#include "thread.h"
#include "src/lib/sleep.h"




int thread_start(thread_t * thread)
{
	pthread_attr_init(&thread->thread_attr);
	int retv = pthread_create(&thread->thread_id, &thread->thread_attr, thread->thread_fn, thread);
	if (0 != retv) {
		fprintf(stderr, "[EE] %s: failed to create thread: %s\n", thread->name, strerror(retv));
		return -1;
	}

	/* Very naive method of checking if a thread has started correctly: wait
	   a bit and check thread's flag. */
	millisleep_nonintr(100);
	if (thread->status != thread_running) {
		fprintf(stderr, "[EE] %s: thread has not started correctly\n", thread->name);
		return -1;
	}

	return 0;
}




int thread_join(thread_t * thread)
{
	pthread_join(thread->thread_id, NULL);

	return 0;
}




int thread_cleanup(thread_t * thread)
{
	pthread_attr_destroy(&thread->thread_attr);

	return 0;
}




