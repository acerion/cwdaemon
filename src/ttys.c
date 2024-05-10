/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#define _DEFAULT_SOURCE /* S_IFMT and related. TODO (acerion) 2021.12.12: check validity of this comment. */

#include "config.h"

#if STDC_HEADERS
# include <stddef.h>
# include <stdlib.h>
# include <string.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_TERMIOS_H
# include <termios.h>
#endif
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#if (defined(__unix__) || defined(unix)) && !defined(USG)
# include <sys/param.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "cwdaemon.h"
#include "log.h"
#include "ttys.h"
#include "utils.h"




/**
   \file ttys.c

   Serial port functions.
*/




static int ttys_init(cwdevice * dev, int fd);
static int ttys_close(cwdevice * dev);
static int ttys_reset_pins_state(cwdevice * dev);
static int ttys_cw(cwdevice * dev, int onoff);
static int ttys_ptt(cwdevice * dev, int onoff);

static int ttys_optparse(cwdevice * dev, const char * option);
static int ttys_optvalidate(cwdevice * dev);




int tty_get_file_descriptor(const char * fname)
{
	char nm[MAXPATHLEN];
	struct stat st;
	int m = 0;

	int retv = build_full_device_path(nm, sizeof (nm), fname);
	if (0 != retv) {
		log_message(LOG_ERR, "Can't build path of tty device from [%s]: %s", fname, strerror(-retv));
		return -1;
	}
	int fd = open(nm, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd == -1) {
		log_message(LOG_ERR, "open() failed for tty device [%s]: %s", nm, strerror(errno)); // TODO (acerion) 2023.04.25: change this to NOTICE
		return (-1);
	}
	if (fstat(fd, &st) == -1) {
		log_message(LOG_ERR, "fstat() failed for tty device [%s]: %s", nm, strerror(errno));
		goto out;
	}
	if ((st.st_mode & S_IFMT) != S_IFCHR) {
		log_message(LOG_ERR, "tty device [%s] is not character device", nm);
		goto out;
	}
	if (ioctl(fd, TIOCMGET, &m) == -1) {
		log_message(LOG_ERR, "ioctl(TIOCMGET) failed for tty device [%s]: %s", nm, strerror(errno));
		goto out;
	}
	return (fd);
out:
	close(fd);
	return (-1);

}




// @reviewed_on{2024.05.09}
int tty_init_cwdevice(cwdevice * dev)
{
	memset(dev, 0, sizeof (cwdevice));

	dev->init                  = ttys_init;
	dev->free                  = ttys_close; // The function name on the right is correct. The cwdevice::free method will be renamed to cwdevice::close in the future.
	dev->reset_pins_state      = ttys_reset_pins_state;
	dev->cw                    = ttys_cw;
	dev->ptt                   = ttys_ptt;

	dev->options.optparse      = ttys_optparse,
	dev->options.optvalidate   = ttys_optvalidate,

	// Set default functions of tty pins. This assignment may be changed by
	// parser of command line options (i.e. by ttys_optparse()).
	dev->options.u.tty_options.key = TIOCM_DTR;
	dev->options.u.tty_options.ptt = TIOCM_RTS;

	// Default name of device file in /dev/.
#if defined(__linux__)
	// "ttyUSB0" seems more likely these days, but I keep "ttyS0" for legacy
	// reasons.
	dev->desc = strdup("ttyS0");
#elif defined(__FreeBSD__)
	dev->desc = strdup("ttyd0");
#elif defined(__OpenBSD__)
	dev->desc = strdup("tty00");
#else
	dev->desc = NULL;
#endif

	return 0;
}




/// Use cwdevice::free() to de-init device that was initialized with this
/// function.
///
/// @revewed_on{2024.05.09}
static int ttys_init(cwdevice * dev, int fd)
{
	dev->fd = fd;
	dev->reset_pins_state(dev);

	return 0;
}




/// @brief Close tty cwdevice
///
/// Use this function to de-initialize a device that was initialized with
/// cwdevice::init().
///
/// @reviewed_on{2024.05.10}
static int ttys_close(cwdevice * dev)
{
	// We will no longer use this device, so let's make sure that its pins
	// are in the same state as they were initially.
	dev->reset_pins_state(dev);

	close(dev->fd);
	dev->fd = -1;

	return 0;
}




/// @brief Reset pins of cwdevice to initial states
///
/// @reviewed_on{2024.05.09}
///
/// @param dev cwdevice to for which to reset the pins
///
/// @return 0
static int ttys_reset_pins_state(cwdevice * dev)
{
	ttys_cw (dev, OFF);
	ttys_ptt (dev, OFF);
	return 0;
}




/// @brief Set "keying" pin of cwdevice according to @p onoff
///
/// @param dev cwdevice on which to change state of "keying" pin
/// @param[in] onoff
///
/// return 0
static int ttys_cw(cwdevice * dev, int onoff)
{
	tty_driver_options const * const dropt = &dev->options.u.tty_options;

	if (dropt->key == 0) {
		// CW keying opted out
		return 0;
	}

	int result = ioctl (dev->fd, onoff ? TIOCMBIS : TIOCMBIC, &dropt->key);
	if (result < 0) {
		cwdaemon_errmsg("Ioctl serial port %s", dev->desc);
		exit (1); // TODO (acerion) 2024.05.09: we shouldn't exit here. Maybe in cwdaemon.c, but not here.
	}
	return 0;
}




/// @brief Set "ptt" pin of cwdevice according to @p onoff
///
/// @reviewed_on{2024.05.09}
///
/// @param dev cwdevice on which to change state of "ptt" pin
/// @param[in] onoff
///
/// return 0
static int ttys_ptt(cwdevice * dev, int onoff)
{
	tty_driver_options const * const dropt = &dev->options.u.tty_options;

	if (dropt->ptt == 0) {
		// PTT opted out
		return 0;
	}

	int result = ioctl (dev->fd, onoff ? TIOCMBIS : TIOCMBIC, &dropt->ptt);
	if (result < 0) {
		cwdaemon_errmsg("Ioctl serial port %s", dev->desc);
		exit (1); // TODO (acerion) 2024.05.09: we shouldn't exit here. Maybe in cwdaemon.c, but not here.
	}
	return 0;
}




/// @brief Parse value passed to "-o" command line option
///
/// Parse the value into configuration of device's pins.
///
/// @param dev cwdevice in which to store parsed configuration
/// @param option string that is a value of "-o" command line option
///
/// @return 0 if option was parsed successfully
/// @return -1 otherwise
static int ttys_optparse(cwdevice * dev, const char * option)
{
	tty_driver_options * const dropt = &dev->options.u.tty_options;

	/* find_opt_value() may be called twice in this function, and each time
	   it will try to find '=' character in 'option'. It's a bit sub-optimal,
	   but this code is not performance-critical. */

	const char * value = NULL;
	if (opt_success == find_opt_value(option, "key", &value)) {
		/* key=DTR|RTS|none */
		if (!strcasecmp(value, "dtr")) {
			dropt->key = TIOCM_DTR;
		} else if (!strcasecmp(value, "rts")) {
			dropt->key = TIOCM_RTS;
		} else if (!strcasecmp(value, "none")) {
			dropt->key = 0;
		} else {
			cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__, "Invalid value for 'key' option: %s", value);
			return -1;
		}
		ttys_cw(dev, 0);
	} else if (opt_success == find_opt_value(option, "ptt", &value)) {
		/* ptt=RTS|DTR|none */
		if (!strcasecmp(value, "dtr")) {
			dropt->ptt = TIOCM_DTR;
		} else if (!strcasecmp(value, "rts")) {
			dropt->ptt = TIOCM_RTS;
		} else if (!strcasecmp(value, "none")) {
			dropt->ptt = 0;
		} else {
			cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__, "Invalid value for 'ptt' option: %s", value);
			return -1;
		}
		ttys_ptt(dev, 0);
	} else {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__, "Invalid option for keying device (expected 'key|ptt=RTS|DTR|none'): [%s]", option);
		return -1;
	}
	return 0;
}




/// @brief Validate parsed driver options
///
/// This function should be called only after all command line options have
/// been parsed. It's possible that configuration of cwdevice was passed in
/// two invocations of "-o" command line option. So only after the parsing of
/// all occurrences of "-o" is done, we may have a full new configuration of
/// cwdevice, and only then we can validate it using this function.
///
/// @reviewed_on{2024.05.09}
///
/// @param dev cwdevice for which to validate configuration of cwdevice
///
/// @return 0 if validation found no errors in the configuration
/// @return -1 if validation found some errors in the configuration
static int ttys_optvalidate(cwdevice * dev)
{
	tty_driver_options const * const dropt = &dev->options.u.tty_options;

	if (0 != dropt->key
	    && 0 != dropt->ptt
	    && dropt->key == dropt->ptt) {
		/* You can't use the same tty pin for two purposes. */
		log_error("keying pin and ptt pin have the same value 0x%02x", dropt->key);
		return -1;
	}

	return 0;
}

