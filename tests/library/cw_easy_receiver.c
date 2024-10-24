/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2024  Kamil Ignacak (acerion@wp.pl)

  This file has been copied from unixcw package (http://unixcw.sourceforge.net/).

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




#ifndef __FreeBSD__
#define _POSIX_C_SOURCE 200809L // clock_gettime() and clock IDs on Alpine Linux 3.20
#endif




#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcw.h>

#include "cw_easy_receiver.h"




#define NANOSECS_PER_MICROSEC 1000


#define get_timer(timer) \
	do { \
		struct timespec clockval = { 0 }; \
		clock_gettime(CLOCK_MONOTONIC, &clockval); \
		timer.tv_sec  = clockval.tv_sec; \
		timer.tv_usec = clockval.tv_nsec / NANOSECS_PER_MICROSEC; \
	} while (0);





cw_easy_rec_t * cw_easy_receiver_new(void)
{
	return (cw_easy_rec_t *) calloc(1, sizeof (cw_easy_rec_t));
}




void cw_easy_receiver_delete(cw_easy_rec_t ** easy_rec)
{
	if (easy_rec && *easy_rec) {
		free(*easy_rec);
		*easy_rec = NULL;
	}
}




void cw_easy_receiver_sk_event(cw_easy_rec_t * easy_rec, int state)
{
	/* Inform xcwcp receiver (which will inform libcw receiver)
	   about new state of straight key ("sk").

	   libcw receiver will process the new state and we will later
	   try to poll a character or space from it. */

	cw_notify_straight_key_event(state);

	return;
}



void cw_easy_rec_handle_keying_event_void(void * easy_receiver, int key_state)
{
	cw_easy_rec_handle_keying_event(easy_receiver, key_state);
}




int cw_easy_rec_handle_keying_event(void * easy_receiver, int key_state)
{
	if (NULL == easy_receiver) {
		fprintf(stderr, "[EE] %s:%d: NULL argument\n", __func__, __LINE__);
		return -1;
	}

	cw_easy_rec_t * easy_rec = (cw_easy_rec_t *) easy_receiver;

	// Ignore calls where the key state matches our tracked key state.
	//fprintf(stderr, "[DD] new key state: %d, tracked key state: %d\n", key_state, easy_rec->tracked_key_state);
	if (key_state == easy_rec->tracked_key_state) {
		fprintf(stderr, "[NN] ignoring new key state, it is the same as tracked key state: %d\n", easy_rec->tracked_key_state);
		// This is *probably* not an error situation, so return zero.
		return 0;
	}
	easy_rec->tracked_key_state = key_state;

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

	// Get timestamp of beginning or of end of mark.
	//
	// The mark_begin/end functions can internally get the mark themselves
	// too (if timestamp pointer arg is NULL), but they would do it using
	// gettimeofday() for legacy reasons. I want the time stamps to be taken
	// from monotonic clock here. This way I avoid issues with ntp.
	struct timeval mark_tstamp = { 0 };
	get_timer(mark_tstamp);

	/* Pass tone state on to the library.  For tone end, check to
	   see if the library has registered any receive error. */
	if (key_state) {
		/* Key down. */
		//fprintf(stderr, "[DD] %10ld.%06ld - mark begin\n", mark_tstamp.tv_sec, mark_tstamp.tv_usec);
		if (!cw_start_receive_tone(&mark_tstamp)) {
			perror("cw_start_receive_tone");
			return -1;
		}
	} else {
		/* Key up. */
		//fprintf(stderr, "[DD] %10ld.%06ld - mark end\n", mark_tstamp.tv_sec, mark_tstamp.tv_usec);
		if (!cw_end_receive_tone(&mark_tstamp)) {
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
				return -1;
			}
		}
	}

	return 0;
}




void cw_easy_receiver_start(cw_easy_rec_t * easy_rec)
{
}




/**
   \brief Poll the CW library receive buffer and handle anything found in the
   buffer
*/
bool cw_easy_receiver_poll(cw_easy_rec_t * easy_rec, int (* callback)(const cw_rec_data_t *))
{
	easy_rec->libcw_receive_errno = 0;

	if (easy_rec->is_pending_iws) {
		/* Check if receiver received the pending inter-word-space. */
		cw_rec_data_t data = { 0 };
		if (CW_SUCCESS == cw_easy_rec_poll_iws_internal(easy_rec, &data)) {
			if (callback) {
				callback(&data);
			}
		}

		if (!easy_rec->is_pending_iws) {
			/* We received the pending space. After it the
			   receiver may have received another
			   character.  Try to get it too. */
			memset(&data, 0, sizeof (data));
			if (cw_easy_receiver_poll_character(easy_rec, &data)) {
				if (callback) {
					callback(&data);
				}
			}
			return true; /* A space has been polled successfully. */
		}
	} else {
		/* Not awaiting a possible space, so just poll the
		   next possible received character. */
		cw_rec_data_t data = { 0 };
		if (cw_easy_receiver_poll_character(easy_rec, &data)) {
			if (callback) {
				callback(&data);
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
int cw_easy_receiver_poll_data(cw_easy_rec_t * easy_rec, cw_rec_data_t * data)
{
	easy_rec->libcw_receive_errno = 0;

	if (easy_rec->is_pending_iws) {
		/* Check if receiver received the pending inter-word-space. */
		cw_easy_rec_poll_iws_internal(easy_rec, data);

		if (!easy_rec->is_pending_iws) {
			/* We received the pending space. After it the
			   receiver may have received another
			   character.  Try to get it too. */
			cw_easy_receiver_poll_character(easy_rec, data);
			return CW_SUCCESS; /* A space has been polled successfully. */
		}
	} else {
		/* Not awaiting a possible space, so just poll the
		   next possible received character. */
		if (cw_easy_receiver_poll_character(easy_rec, data)) {
			return CW_SUCCESS; /* A character has been polled successfully. */
		}
	}

	return CW_FAILURE; /* Nothing was polled at this time. */
}





/**
   @brief Try polling a character from receiver

   See if a receiver has received/recognized a character (a character other
   than ' ').

   Function may return false (failure) for completely valid reasons, e.g.
   when it's too early to decide if a receiver has received something or not.

   Call this function periodically on a receiver.

   @param[in] easy_rec Easy receiver from which to try to poll the character
   @param[out] data Data of easy receiver, filled on successful poll.

   @return CW_SUCCESS if receiver has received a character (@p data is updated accordingly)
   @return CW_FAILURE if receiver didn't receive a character
*/
bool cw_easy_receiver_poll_character(cw_easy_rec_t * easy_rec, cw_rec_data_t * data)
{
	// This timer will be used by poll function to measure current duration
	// of space that is happening after a current character. The space may be
	// inter-word-space, but also may already be inter-word-space.
	struct timeval timer;
	get_timer(timer);

	//fprintf(stderr, "[DD] %10ld.%06ld - poll for character\n", timer.tv_sec, timer.tv_usec);
	errno = 0;
	const bool received = cw_receive_character(&timer, &data->character, &data->is_iws, NULL);
	data->errno_val = errno;
	if (received) {

		/* A full character has been received. Directly after
		   it comes a space. Either a short inter-character
		   space followed by another character (in this case
		   we won't display the inter-character space), or
		   longer inter-word space - this space we would like
		   to catch and display.

		   Set a flag indicating that next poll may result in
		   inter-word space. */
		easy_rec->is_pending_iws = true;

		//fprintf(stderr, "[DD] Received character '%c', is_iws = %d\n", data->character, data->is_iws);

		return CW_SUCCESS;

	} else {
		/* Handle receive error detected on trying to read a character. */
		switch (data->errno_val) {
		case EAGAIN:
			//fprintf(stderr, "EAGAIN\n");
			/* Call made too early, receiver hasn't
			   received a full character yet. Try next
			   time. */
			break;

		case ERANGE:
			//fprintf(stderr, "ERANGE\n");
			/* Call made not in time, or not in proper
			   sequence. Receiver hasn't received any
			   character (yet). Try harder. */
			break;

		case ENOENT:
			fprintf(stderr, "ENOENT\n");
			/* Invalid character in receiver's buffer. */
			cw_clear_receive_buffer();
			break;

		case EINVAL:
			fprintf(stderr, "EINVAL\n");
			/* Timestamp error. */
			cw_clear_receive_buffer();
			break;

		default:
			perror("cw_receive_character");
		}

		return CW_FAILURE;
	}
}




/**
   @brief Try polling an inter-word-space from receiver

   See if a receiver has received/recognized an inter-word-space (a ' ' character).

   'iws' in function name stands for 'inter-word-space' (i.e. a ' ' character).

   Function may return false (failure) for completely valid reasons, e.g.
   when it's too early to decide if a receiver has received something or not.

   Call this function if cw_easy_rec_t::is_pending_iws flag is set to true.

   @param[in] easy_rec Easy receiver from which to try to poll the character
   @param[out] data Data of easy receiver, filled on successful poll.

   @return CW_SUCCESS if receiver has received a space (@p data is updated accordingly)
   @return CW_FAILURE if receiver didn't receive a space
*/
int cw_easy_rec_poll_iws_internal(cw_easy_rec_t * easy_rec, cw_rec_data_t * data)
{
	/* We expect the receiver to contain a character, but we don't
	   ask for it this time. The receiver should also store
	   information about an inter-character space. If it is longer
	   than a regular inter-character space, then the receiver
	   will treat it as inter-word space, and communicate it over
	   is_iws. */

	// This timer will be used by poll function to measure duration of
	// current space (space from end of last mark till now).
	struct timeval timer;
	get_timer(timer);

	//fprintf(stderr, "[DD] %10ld.%06ld - poll for iws\n", timer.tv_sec, timer.tv_usec);
	cw_receive_character(&timer, &data->character, &data->is_iws, NULL);
	if (data->is_iws) {
		//fprintf(stderr, "[DD] Character at inter-word-space: '%c'\n", data->character);

		cw_clear_receive_buffer();
		easy_rec->is_pending_iws = false;
		return CW_SUCCESS; /* Inter-word-space has been polled. */
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
		return CW_FAILURE; /* Inter-word-space has not been polled. */
	}
}




int cw_easy_rec_get_libcw_errno(const cw_easy_rec_t * easy_rec)
{
	return easy_rec->libcw_receive_errno;
}




void cw_easy_rec_clear_libcw_errno(cw_easy_rec_t * easy_rec)
{
	easy_rec->libcw_receive_errno = 0;
}




bool cw_easy_rec_is_pending_inter_word_space(const cw_easy_rec_t * easy_rec)
{
	return easy_rec->is_pending_iws;
}




void cw_easy_receiver_clear(cw_easy_rec_t * easy_rec)
{
	cw_clear_receive_buffer();
	easy_rec->is_pending_iws = false;
	easy_rec->libcw_receive_errno = 0;
	easy_rec->tracked_key_state = false;
}




int cw_easy_receiver_on_key_state_change(void * arg_easy_rec, int key_state)
{
	cw_easy_rec_t * easy_rec = (cw_easy_rec_t *) arg_easy_rec;
	cw_easy_receiver_sk_event(easy_rec, key_state);

	// fprintf(stdout, "[INFO ] easy receiver: key is %s\n", key_state ? "down" : "up");

	return 0;
}



#if 0

cw_ret_t cw_easy_rec_set_speed(cw_easy_rec_t * easy_rec, int speed)
{
	if (NULL == easy_rec) {
		fprintf(stderr, "[ERROR] %s:%d: NULL argument\n", __func__, __LINE__);
		return CW_FAILURE;
	}
	return cw_rec_set_speed(easy_rec->rec, speed);
}




cw_ret_t cw_easy_rec_get_speed(cw_easy_rec_t * easy_rec, float * speed)
{
	if (NULL == easy_rec || NULL == speed) {
		fprintf(stderr, "[ERROR] %s:%d: NULL argument\n", __func__, __LINE__);
		return CW_FAILURE;
	}
	*speed = cw_rec_get_speed(easy_rec->rec);
	return CW_SUCCESS;
}




cw_ret_t cw_easy_rec_set_tolerance(cw_easy_rec_t * easy_rec, int tolerance)
{
	if (NULL == easy_rec) {
		fprintf(stderr, "[ERROR] %s:%d: NULL argument\n", __func__, __LINE__);
		return CW_FAILURE;
	}
	return cw_rec_set_tolerance(easy_rec->rec, tolerance);
}



int cw_easy_rec_get_tolerance(const cw_easy_rec_t * easy_rec)
{
	if (NULL == easy_rec) {
		fprintf(stderr, "[EE] %s:%d: NULL argument\n", __func__, __LINE__);
		return CW_FAILURE;
	}
	return cw_rec_get_tolerance(easy_rec->rec);
}




void cw_easy_rec_register_receive_callback(cw_easy_rec_t * easy_rec, cw_easy_rec_receive_callback_t cb, void * data)
{
	easy_rec->receive_callback = cb;
	easy_rec->receive_callback_data = data;
}




cw_ret_t cw_easy_rec_init_tracked_key_state(cw_easy_rec_t * rec, int key_state)
{
	rec->tracked_key_state = key_state;
	return CW_SUCCESS;
}

#endif

