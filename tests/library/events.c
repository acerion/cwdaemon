/*
 * This file is a part of cwdaemon project.
 *
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




/**
   Functions and data structures for events, used in for cwdaemon tests.

   Events encapsulate actions performed by cwdaemon server that reflect
   functionalities of cwdaemon.

   The actions can include:
    - keying a Morse code on cwdevice,
    - toggling PTT pin on cwdevice,
    - sending back reply to server's network client.

   Recording the actions and confirming that they occur in proper order and
   at proper interval is vital for verification and validation of cwdaemon's
   behaviour.
*/




#include <stdio.h>
#include <string.h>

#include "tests/library/events.h"
#include "tests/library/misc.h"




void events_print(const events_t * events)
{
	const struct timespec first_ts = events->events[0].tstamp;


	const size_t n_events = sizeof (events->events) / sizeof (events->events[0]);
	for (size_t e = 0; e < n_events; e++) {

		char escaped[64] = { 0 };

		const event_t * event = &events->events[e];

		if (event->event_type == event_type_none) {
			/* End of events. */
			break;
		}
		const struct timespec ts = event->tstamp;
		const long int sec = ts.tv_sec - first_ts.tv_sec; /* Normalize. Make the timestamps more readable. */
		switch (event->event_type) {
		case event_type_morse_receive:
			fprintf(stderr, "[DD] event #%02zd: %3ld.%09ld: Morse receive:  [%s]\n",
			        e,
			        sec, ts.tv_nsec,
			        event->u.morse_receive.string);
			break;
		case event_type_client_socket_receive:
			fprintf(stderr, "[DD] event #%02zd: %3ld.%09ld: socket receive: [%s]\n",
			        e,
			        sec, ts.tv_nsec,
			        escape_string(event->u.socket_receive.string, escaped, sizeof (escaped)));
			break;
		case event_type_sigchld:
			fprintf(stderr, "[DD] event #%02zd: %3ld.%09ld: SIGCHLD: wstatus = %2x\n",
			        e,
			        sec, ts.tv_nsec,
			        event->u.sigchld.wstatus);
			break;
		case event_type_request_exit:
			fprintf(stderr, "[DD] event #%02zd: %3ld.%09ld: EXIT request\n",
			        e,
			        sec, ts.tv_nsec);
			break;
		case event_type_none:
		default:
			break;
		}
	}
}




void events_clear(events_t * events)
{
	memset(events->events, 0, sizeof (events->events));
	events->event_idx = 0;

	/* Don't touch events::mutex. */

}
