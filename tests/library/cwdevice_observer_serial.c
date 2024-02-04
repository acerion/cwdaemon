/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2022 - 2023 Kamil Ignacak <acerion@wp.pl>
 *
 * Code for polling of serial port is based on "statserial - Serial Port
 * Status Utility" (GPL2+).
 * Copyright (C) 1994 Jeff Tranter (Jeff_Tranter@Mitel.COM)
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




/**
   @file Code that observes cwdaemon's tty cwdevice

   Code for polling of serial port is based on "statserial" program (see
   copyright notice above).
*/




// TODO (acerion) 2022.12.17: GNU's strerror_r() may not fill provided buffer :(
// Write own replacement of strerror_r() that bevaves as expected.
//
// I don't want to be guessing which version of strerror_r() is being
// used at compile time: XSI variant or GNU variant.
#define _GNU_SOURCE /* strerror_r() */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "cw_rec_utils.h"
#include "cwdevice_observer_serial.h"
#include "log.h"




#define PTT_EXPERIMENT 1

#if PTT_EXPERIMENT
typedef struct ptt_sink_t {
	int dummy;
} ptt_sink_t;




static ptt_sink_t g_ptt_sink = { 0 };




/**
   @brief Inform a ptt sink that a ptt pin has a new state (on or off)

   A simple wrapper that seems to be convenient.
*/
static bool on_ptt_state_change(void * arg_ptt_sink, bool ptt_is_on)
{
	__attribute__((unused)) ptt_sink_t * ptt_sink = (ptt_sink_t *) arg_ptt_sink;
	test_log_debug("cwdevice observer: ptt sink: ptt is %s\n", ptt_is_on ? "on" : "off");

	return true;
}
#endif /* #if PTT_EXPERIMENT */




bool cwdevice_observer_serial_open(cwdevice_observer_t * observer)
{
	/* Open serial port. */
	errno = 0;
	int fd = open(observer->source_path, O_RDONLY);
	if (fd == -1) {
		char buf[32] = { 0 };
		char * b = strerror_r(errno, buf, sizeof (buf));
		test_log_err("cwdevice observer: open(%s): %s / %d\n", observer->source_path, b, errno);
		return false;
	}
	observer->source_reference = (uintptr_t) fd;

	return true;
}




void cwdevice_observer_serial_close(cwdevice_observer_t * observer)
{
	int fd = (int) observer->source_reference;
	close(fd);
}




bool cwdevice_observer_serial_poll_once(cwdevice_observer_t * observer, bool * key_is_down, bool * ptt_is_on)
{
	int fd = (int) observer->source_reference;
	errno = 0;
	unsigned int value = 0;
	int status = ioctl(fd, TIOCMGET, &value);
	if (status != 0) {
		char buf[32] = { 0 };
		char * b = strerror_r(errno, buf, sizeof (buf));
		test_log_err("cwdevice observer: ioctl(TIOCMGET): %s / %d\n", b, errno);
		return false;
	}
	unsigned int keying_pin = TIOCM_DTR; /* Default ID of keying pin. */
	unsigned int ptt_pin = TIOCM_RTS;    /* Default ID of ptt pin. */
	if (observer->tty_pins_config.explicit) {
		/* Configuration of observer includes explicitly specified IDs for
		   tty pins. Use them here. */
		keying_pin = observer->tty_pins_config.pin_keying;
		ptt_pin    = observer->tty_pins_config.pin_ptt;
	}
	*key_is_down = !!(value & keying_pin);
	*ptt_is_on   = !!(value & ptt_pin);
	return true;
}




/**
   @brief Inform an easy receiver that a key has a new state (up or down)

   A simple wrapper that seems to be convenient.
*/
static bool on_key_state_change(void * arg_easy_rec, bool key_is_down)
{
	cw_easy_receiver_t * easy_rec = (cw_easy_receiver_t *) arg_easy_rec;
	cw_easy_receiver_sk_event(easy_rec, key_is_down);
	// test_log_debug("cwdevice observer: key is %s\n", key_is_down ? "down" : "up");

	return true;
}




int cwdevice_observer_tty_setup(cwdevice_observer_t * observer, void * key_state_cb_arg, const tty_pins_t * observer_pins_config)
{
	memset(observer, 0, sizeof (cwdevice_observer_t));

	observer->open_fn  = cwdevice_observer_serial_open;
	observer->close_fn = cwdevice_observer_serial_close;
	observer->new_key_state_cb     = on_key_state_change;
	observer->new_key_state_cb_arg = key_state_cb_arg;
	if (observer_pins_config) {
		observer->tty_pins_config = *observer_pins_config;
	}

	snprintf(observer->source_path, sizeof (observer->source_path), "%s", "/dev/" TEST_TTY_CWDEVICE_NAME);

#if PTT_EXPERIMENT
	observer->new_ptt_state_cb  = on_ptt_state_change;
	observer->new_ptt_state_arg = &g_ptt_sink;
#endif

	cwdevice_observer_configure_polling(observer, 0, cwdevice_observer_serial_poll_once);

	if (!observer->open_fn(observer)) {
		test_log_err("cwdevice observer: failed to open cwdevice '%s' in setup of observer\n", observer->source_path);
		return -1;
	}

	return 0;
}




