/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2022  Kamil Ignacak (acerion@wp.pl)

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc., 51
  Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/




#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcw.h>

#include "cw_rec_utils.h"




cw_easy_receiver_t * cw_easy_receiver_new(void)
{
	return (cw_easy_receiver_t *) calloc(1, sizeof (cw_easy_receiver_t));
}




void cw_easy_receiver_delete(cw_easy_receiver_t ** easy_rec)
{
	if (easy_rec && *easy_rec) {
		free(*easy_rec);
		*easy_rec = NULL;
	}
}




void cw_easy_receiver_sk_event(cw_easy_receiver_t * easy_rec, bool is_down)
{
	/* Inform xcwcp receiver (which will inform libcw receiver)
	   about new state of straight key ("sk").

	   libcw receiver will process the new state and we will later
	   try to poll a character or space from it. */

	//fprintf(stderr, "Callback function, key state = %d\n", key_state);


	/* Prepare timestamp for libcw on both "key up" and "key down"
	   events. There is no code in libcw that would generate
	   updated consecutive timestamps for us (as it does in case
	   of iambic keyer).

	   TODO: see in libcw how iambic keyer updates a timer, and
	   how straight key does not. Apparently the timer is used to
	   recognize and distinguish dots from dashes. Maybe straight
	   key could have such timer as well? */
	gettimeofday(&easy_rec->main_timer, NULL);
	//fprintf(stderr, "time on Skey down:  %10ld : %10ld\n", easy_rec->main_timer.tv_sec, easy_rec->main_timer.tv_usec);

	cw_notify_straight_key_event(is_down);

	return;
}




void cw_easy_receiver_ik_left_event(cw_easy_receiver_t * easy_rec, bool is_down, bool is_reverse_paddles)
{
	easy_rec->is_left_down = is_down;
	if (easy_rec->is_left_down && !easy_rec->is_right_down) {
		/* Prepare timestamp for libcw, but only for initial
		   "paddle down" event at the beginning of
		   character. Don't create the timestamp for any
		   successive "paddle down" events inside a character.

		   In case of iambic keyer the timestamps for every
		   next (non-initial) "paddle up" or "paddle down"
		   event in a character will be created by libcw.

		   TODO: why libcw can't create such timestamp for
		   first event for us? */
		gettimeofday(&easy_rec->main_timer, NULL);
		//fprintf(stderr, "time on Lkey down:  %10ld : %10ld\n", easy_rec->main_timer.tv_sec, easy_rec->main_timer.tv_usec);
	}

	/* Inform libcw about state of left paddle regardless of state
	   of the other paddle. */
	is_reverse_paddles
		? cw_notify_keyer_dash_paddle_event(is_down)
		: cw_notify_keyer_dot_paddle_event(is_down);
	return;
}





void cw_easy_receiver_ik_right_event(cw_easy_receiver_t * easy_rec, bool is_down, bool is_reverse_paddles)
{
	easy_rec->is_right_down = is_down;
	if (easy_rec->is_right_down && !easy_rec->is_left_down) {
		/* Prepare timestamp for libcw, but only for initial
		   "paddle down" event at the beginning of
		   character. Don't create the timestamp for any
		   successive "paddle down" events inside a character.

		   In case of iambic keyer the timestamps for every
		   next (non-initial) "paddle up" or "paddle down"
		   event in a character will be created by libcw. */
		gettimeofday(&easy_rec->main_timer, NULL);
		//fprintf(stderr, "time on Rkey down:  %10ld : %10ld\n", easy_rec->main_timer.tv_sec, easy_rec->main_timer.tv_usec);
	}

	/* Inform libcw about state of left paddle regardless of state
	   of the other paddle. */
	is_reverse_paddles
		? cw_notify_keyer_dot_paddle_event(is_down)
		: cw_notify_keyer_dash_paddle_event(is_down);
	return;
}





/**
   \brief Handler for the keying callback from the CW library
   indicating that the state of a key has changed.

   The "key" is libcw's internal key structure. It's state is updated
   by libcw when e.g. one iambic keyer paddle is constantly
   pressed. It is also updated in other situations. In any case: the
   function is called whenever state of this key changes.

   Notice that the description above talks about a key, not about a
   receiver. Key's states need to be interpreted by receiver, which is
   a separate task. Key and receiver are separate concepts. This
   function connects them.

   This function, called on key state changes, calls receiver
   functions to ensure that receiver does "receive" the key state
   changes.

   This function is called in signal handler context, and it takes
   care to call only functions that are safe within that context.  In
   particular, it goes out of its way to deliver results by setting
   flags that are later handled by receive polling.
*/
void cw_easy_receiver_handle_libcw_keying_event(void * easy_receiver, int key_state)
{
	cw_easy_receiver_t * easy_rec = (cw_easy_receiver_t *) easy_receiver;
	/* Ignore calls where the key state matches our tracked key
	   state.  This avoids possible problems where this event
	   handler is redirected between application instances; we
	   might receive an end of tone without seeing the start of
	   tone. */
	if (key_state == easy_rec->tracked_key_state) {
		//fprintf(stderr, "tracked key state == %d\n", easy_rec->tracked_key_state);
		return;
	} else {
		//fprintf(stderr, "tracked key state := %d\n", key_state);
		easy_rec->tracked_key_state = key_state;
	}

	/* If this is a tone start and we're awaiting an inter-word
	   space, cancel that wait and clear the receive buffer. */
	if (key_state && easy_rec->is_pending_iws) {
		/* Tell receiver to prepare (to make space) for
		   receiving new character. */
		cw_clear_receive_buffer();

		/* The tone start means that we're seeing the next
		   incoming character within the same word, so no
		   inter-word space is possible at this point in
		   time. The space that we were observing/waiting for,
		   was just inter-character space. */
		easy_rec->is_pending_iws = false;
	}

	//fprintf(stderr, "calling callback, stage 2\n");

	/* Pass tone state on to the library.  For tone end, check to
	   see if the library has registered any receive error. */
	if (key_state) {
		/* Key down. */
		//fprintf(stderr, "start receive tone: %10ld . %10ld\n", easy_rec->main_timer->tv_sec, easy_rec->main_timer->tv_usec);
		if (!cw_start_receive_tone(&easy_rec->main_timer)) {
			// TODO: Perhaps this should be counted as test error
			perror("cw_start_receive_tone");
			return;
		}
	} else {
		/* Key up. */
		//fprintf(stderr, "end receive tone:   %10ld . %10ld\n", easy_rec->main_timer->tv_sec, easy_rec->main_timer->tv_usec);
		if (!cw_end_receive_tone(&easy_rec->main_timer)) {
			/* Handle receive error detected on tone end.
			   For ENOMEM and ENOENT we set the error in a
			   class flag, and display the appropriate
			   message on the next receive poll. */
			switch (errno) {
			case EAGAIN:
				/* libcw treated the tone as noise (it
				   was shorter than noise threshold).
				   No problem, not an error. */
				break;
			case ENOMEM:
			case ERANGE:
			case EINVAL:
			case ENOENT:
				easy_rec->libcw_receive_errno = errno;
				cw_clear_receive_buffer();
				break;
			default:
				perror("cw_end_receive_tone");
				// TODO: Perhaps this should be counted as test error
				return;
			}
		}
	}

	return;
}




void cw_easy_receiver_start(cw_easy_receiver_t * easy_rec)
{
	/* The call above registered receiver->main_timer as a generic
	   argument to a callback. However, libcw needs to know when
	   the argument happens to be of type 'struct timeval'. This
	   is why we have this second call, explicitly passing
	   receiver's timer to libcw. */
	cw_iambic_keyer_register_timer(&easy_rec->main_timer);

	gettimeofday(&easy_rec->main_timer, NULL);
	//fprintf(stderr, "time on aux config: %10ld : %10ld\n", easy_rec->main_timer.tv_sec, easy_rec->main_timer.tv_usec);
}




/**
   \brief Poll the CW library receive buffer and handle anything found in the
   buffer
*/
bool cw_easy_receiver_poll(cw_easy_receiver_t * easy_rec, int (* callback)(const cw_rec_data_t *))
{
	easy_rec->libcw_receive_errno = 0;

	if (easy_rec->is_pending_iws) {
		/* Check if receiver received the pending inter-word-space. */
		cw_rec_data_t erd = { 0 };
		if (cw_easy_receiver_poll_space(easy_rec, &erd)) {
			if (callback) {
				callback(&erd);
			}
		}

		if (!easy_rec->is_pending_iws) {
			/* We received the pending space. After it the
			   receiver may have received another
			   character.  Try to get it too. */
			memset(&erd, 0, sizeof (erd));
			if (cw_easy_receiver_poll_character(easy_rec, &erd)) {
				if (callback) {
					callback(&erd);
				}
			}
			return true; /* A space has been polled successfully. */
		}
	} else {
		/* Not awaiting a possible space, so just poll the
		   next possible received character. */
		cw_rec_data_t erd = { 0 };
		if (cw_easy_receiver_poll_character(easy_rec, &erd)) {
			if (callback) {
				callback(&erd);
			}
			return true; /* A character has been polled successfully. */
		}
	}

	return false; /* Nothing was polled at this time. */
}




/**
   \brief Poll the CW library receive buffer and handle anything found in the
   buffer
*/
bool cw_easy_receiver_poll_data(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd)
{
	easy_rec->libcw_receive_errno = 0;

	if (easy_rec->is_pending_iws) {
		/* Check if receiver received the pending inter-word-space. */
		cw_easy_receiver_poll_space(easy_rec, erd);

		if (!easy_rec->is_pending_iws) {
			/* We received the pending space. After it the
			   receiver may have received another
			   character.  Try to get it too. */
			cw_easy_receiver_poll_character(easy_rec, erd);
			return true; /* A space has been polled successfully. */
		}
	} else {
		/* Not awaiting a possible space, so just poll the
		   next possible received character. */
		if (cw_easy_receiver_poll_character(easy_rec, erd)) {
			return true; /* A character has been polled successfully. */
		}
	}

	return false; /* Nothing was polled at this time. */
}




bool cw_easy_receiver_poll_character(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd)
{
	/* Don't use receiver.easy_rec->main_timer - it is used exclusively for
	   marking initial "key down" events. Use local throw-away
	   timer.

	   Additionally using reveiver.easy_rec->main_timer here would mess up time
	   intervals measured by receiver.easy_rec->main_timer, and that would
	   interfere with recognizing dots and dashes. */
	struct timeval timer;
	gettimeofday(&timer, NULL);
	//fprintf(stderr, "poll_receive_char:  %10ld : %10ld\n", timer.tv_sec, timer.tv_usec);

	errno = 0;
	const bool received = cw_receive_character(&timer, &erd->character, &erd->is_iws, NULL);
	erd->errno_val = errno;
	if (received) {

#ifdef XCWCP_WITH_REC_TEST
		if (CW_SUCCESS != cw_rec_tester_on_character(easy_rec->rec_tester, erd, &timer)) {
			/* FIXME acerion 2022.02.19: this is a library code,
			   so don't call exit(). */
			exit(EXIT_FAILURE);
		}
#endif
		/* A full character has been received. Directly after
		   it comes a space. Either a short inter-character
		   space followed by another character (in this case
		   we won't display the inter-character space), or
		   longer inter-word space - this space we would like
		   to catch and display.

		   Set a flag indicating that next poll may result in
		   inter-word space. */
		easy_rec->is_pending_iws = true;

		//fprintf(stderr, "Received character '%c'\n", erd->character);

		return true;

	} else {
		/* Handle receive error detected on trying to read a character. */
		switch (erd->errno_val) {
		case EAGAIN:
			/* Call made too early, receiver hasn't
			   received a full character yet. Try next
			   time. */
			break;

		case ERANGE:
			/* Call made not in time, or not in proper
			   sequence. Receiver hasn't received any
			   character (yet). Try harder. */
			break;

		case ENOENT:
			/* Invalid character in receiver's buffer. */
			cw_clear_receive_buffer();
			break;

		case EINVAL:
			/* Timestamp error. */
			cw_clear_receive_buffer();
			break;

		default:
			perror("cw_receive_character");
		}

		return false;
	}
}




// TODO: can we return true when a space has been successfully polled,
// instead of returning it through erd?
bool cw_easy_receiver_poll_space(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd)
{
	/* We expect the receiver to contain a character, but we don't
	   ask for it this time. The receiver should also store
	   information about an inter-character space. If it is longer
	   than a regular inter-character space, then the receiver
	   will treat it as inter-word space, and communicate it over
	   is_iws.

	   Don't use receiver.easy_rec->main_timer - it is used eclusively for
	   marking initial "key down" events. Use local throw-away
	   timer. */
	struct timeval timer;
	gettimeofday(&timer, NULL);
	//fprintf(stderr, "poll_space(): %10ld : %10ld\n", timer.tv_sec, timer.tv_usec);

	cw_receive_character(&timer, &erd->character, &erd->is_iws, NULL);
	if (erd->is_iws) {
		//fprintf(stderr, "End of word '%c'\n\n", erd->character);

#ifdef XCWCP_WITH_REC_TEST
		if (CW_SUCCESS != cw_rec_tester_on_space(easy_rec->rec_tester, erd, &timer)) {
			/* FIXME acerion 2022.02.19: this is a library code,
			   so don't call exit(). */
			exit(EXIT_FAILURE);
		}
#endif

		cw_clear_receive_buffer();
		easy_rec->is_pending_iws = false;
		return true; /* Inter-word-space has been polled. */
	} else {
		/* We don't reset easy_rec->is_pending_iws. The
		   space that currently lasts, and isn't long enough
		   to be considered inter-word space, may grow to
		   become the inter-word space. Or not.

		   This growing of inter-character space into
		   inter-word space may be terminated by incoming next
		   tone (key down event) - the tone will mark
		   beginning of new character within the same
		   word. And since a new character begins, the flag
		   will be reset (elsewhere). */
		return false; /* Inter-word-space has not been polled. */
	}
}




int cw_easy_receiver_get_libcw_errno(const cw_easy_receiver_t * easy_rec)
{
	return easy_rec->libcw_receive_errno;
}




void cw_easy_receiver_clear_libcw_errno(cw_easy_receiver_t * easy_rec)
{
	easy_rec->libcw_receive_errno = 0;
}




bool cw_easy_receiver_is_pending_inter_word_space(const cw_easy_receiver_t * easy_rec)
{
	return easy_rec->is_pending_iws;
}




void cw_easy_receiver_clear(cw_easy_receiver_t * easy_rec)
{
	cw_clear_receive_buffer();
	easy_rec->is_pending_iws = false;
	easy_rec->libcw_receive_errno = 0;
	easy_rec->tracked_key_state = false;
}




