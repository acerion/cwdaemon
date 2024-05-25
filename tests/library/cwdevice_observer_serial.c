/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2022 - 2024 Kamil Ignacak <acerion@wp.pl>
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
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/// @file
///
/// Code that observes cwdaemon's tty cwdevice
///
/// The observation happens from within the Linux system. The observed device
/// is Linux's /dev/ttyXY device or similar device. This file doesn't
/// implement observing of a cwdevice "from outside" of system on which the
/// test code is running.
///
/// For such observation "from outside" we would need to use some kind of
/// external hardware (loopback?) that would monitor the physical pins of
/// hardware port and feed that info to test code.
///
/// Code for polling of serial port is based on "statserial" program (see
/// copyright notice above).




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

#include "cw_easy_receiver.h"
#include "cwdevice_observer_serial.h"
#include "log.h"
#include "test_defines.h"




int cwdevice_observer_serial_open(cwdevice_observer_t * observer)
{
	/* Open serial port. */
	errno = 0;
	int fd = open(observer->source_path, O_RDONLY);
	if (fd == -1) {
		char buf[ERRNO_BUF_SIZE] = { 0 };
		char * b = strerror_r(errno, buf, sizeof (buf));
		test_log_err("cwdevice observer: open(%s): %s / %d\n", observer->source_path, b, errno);
		observer->source_reference = (uintptr_t) -1;
		return -1;
	}

	observer->source_reference = (uintptr_t) fd;
	return 0;
}




void cwdevice_observer_serial_close(cwdevice_observer_t * observer)
{
	int fd = (int) observer->source_reference;
	if (fd != -1) {
		close(fd);
		observer->source_reference = (uintptr_t) -1;
	}
}




int cwdevice_observer_serial_poll_once(cwdevice_observer_t * observer, bool * key_is_down, bool * ptt_is_on)
{
	int fd = (int) observer->source_reference;
	errno = 0;
	unsigned int value = 0;
	int status = ioctl(fd, TIOCMGET, &value);
	if (status != 0) {
		char buf[ERRNO_BUF_SIZE] = { 0 };
		char * b = strerror_r(errno, buf, sizeof (buf));
		test_log_err("cwdevice observer: ioctl(TIOCMGET): %s / %d\n", b, errno);
		return -1;
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

	return 0;
}




// @reviewed_on{2024.05.11}
int cwdevice_observer_tty_setup(cwdevice_observer_t * observer, const tty_pins_t * observer_pins_config)
{
	memset(observer, 0, sizeof (cwdevice_observer_t));

	observer->open_fn  = cwdevice_observer_serial_open;
	observer->close_fn = cwdevice_observer_serial_close;
	if (observer_pins_config) {
		observer->tty_pins_config = *observer_pins_config;
	}

	// "name" -> "/dev/name"
	cwdevice_get_full_path(TESTS_TTY_CWDEVICE_NAME, observer->source_path, sizeof (observer->source_path));

	cwdevice_observer_configure_polling(observer, 0, cwdevice_observer_serial_poll_once);

	return 0;
}




