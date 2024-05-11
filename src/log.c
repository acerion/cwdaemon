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

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "cwdaemon.h"
#include "log.h"




#define LOG_BUF_SIZE (1024 + 1)




extern bool g_forking;
extern FILE *cwdaemon_debug_f;
extern char *cwdaemon_debug_f_path;
extern options_t g_current_options;




/**
   @brief Get string label corresponding to given log priority

   @reviewed_on{2024.05.10}
*/
char const * log_get_priority_label(int priority)
{
	switch (priority) {
	case LOG_ERR:
		return "EE";
	case LOG_WARNING:
		return "WW";
	case LOG_NOTICE:
		return "NN";
	case LOG_INFO:
		return "II";
	case LOG_DEBUG:
		return "DD";
	default:
	case LOG_CRIT:    // == None (don't display logs).
		return "??";
	}
}




/**
   \brief Print error string to the console or syslog

   Function checks if cwdaemon has forked, and prints given error string
   to stdout (if cwdaemon hasn't forked) or to syslog (if cwdaemon has
   forked).

   Function accepts printf-line formatting string as first argument, and
   a set of optional arguments to be inserted into the formatting string.

   \param format - first part of an error string, a formatting string
*/
void cwdaemon_errmsg(const char *format, ...)
{
	va_list ap;
	char s[LOG_BUF_SIZE] = { 0 };

	va_start(ap, format);
	vsnprintf(s, sizeof (s), format, ap);
	va_end(ap);

	if (g_forking) {
		syslog(LOG_ERR, "%s\n", s);
	} else {
		printf("%s: %s failed: \"%s\"\n", PACKAGE, s, strerror(errno));
		fflush(stdout);
	}

	return;
}




// @reviewed_on{2024.05.10}
void log_message(int priority, char const * format, ...)
{
	// If we fork and detach from console, we can't log to stdout/stderr, and
	// we can log only to syslog.
	const bool to_syslog = g_forking;

	if (!to_syslog && NULL == cwdaemon_debug_f) {
		// We don't want to log to either syslog or to file.
		return;
	}

	// LOG_EMERG == 0, ... LOG_DEBUG == 7.
	if (priority > g_current_options.log_threshold) {
		return;
	}

	va_list ap;
	char buf[LOG_BUF_SIZE] = { 0 };
	va_start(ap, format);
	vsnprintf(buf, sizeof (buf), format, ap);
	va_end(ap);

	if (to_syslog) {
		// TODO (acerion) 2024.05.10: double-check if "\n" is allowed for
		// syslog messages.
		syslog(priority, "%s\n", buf);
	} else {
		// If not to syslog then the other option is to log to file.
		char const * prio_str = log_get_priority_label(priority);
		fprintf(cwdaemon_debug_f, "[%s] %s: %s\n", prio_str, PACKAGE, buf);
	}

	return;
}




/**
   \brief Print debug string to debug file

   Function decides if given \p verbosity level is sufficient to print
   given \p format debug string, and prints it to predefined file. If
   current global verbosity level is "None", no information will be
   printed.

   Currently \p verbosity can have one of values defined in enum
   cwdaemon_verbosity.

   Function accepts printf-line formatting string as last named
   argument (\p format), and a set of optional arguments to be
   inserted into the formatting string.

   @reviewed_on{2024.05.10}

   \param verbosity - verbosity level of given debug string
   \param format - formatting string of a debug string being printed
*/
void cwdaemon_debug(int verbosity, __attribute__((unused)) const char *func, __attribute__((unused)) int line, const char *format, ...)
{
	if (!cwdaemon_debug_f) {
		return;
	}

	/* LOG_EMERG == 0, ... LOG_DEBUG == 7. */
	if (verbosity > g_current_options.log_threshold) {
		return;
	}

	va_list ap;
	char s[LOG_BUF_SIZE];
	va_start(ap, format);
	vsnprintf(s, sizeof (s), format, ap);
	va_end(ap);

	fprintf(cwdaemon_debug_f, "[%s] %s: %s\n", log_get_priority_label(verbosity), PACKAGE, s);
	// fprintf(cwdaemon_debug_f, "cwdaemon:        %s(): %d\n", func, line);
	fflush(cwdaemon_debug_f);


	return;
}




/**
   \brief Configure file descriptor for debug output
 */
void cwdaemon_debug_open(bool forking)
{
	/* First handle a clash of command line arguments. */
	if (forking) {
		if (cwdaemon_debug_f_path
		    && (!strcmp(cwdaemon_debug_f_path, "stdout")
			|| !strcmp(cwdaemon_debug_f_path, "stderr"))) {

			/* If the path is not specified (path is
			   NULL), it may imply that the debug output
			   should be stdout. But since we explicitly
			   asked for forking, the implied debug output
			   to stdout will be ignored. This is a valid
			   situation: explicit request for forking
			   overrides implicit expectation of debug
			   file being stdout.

			   On the other hand, if we explicitly asked
			   for debug output to be stdout or stderr,
			   and at the same time explicitly asked for
			   forking, that is completely different
			   matter. This is an error (clash of command
			   line arguments). */

			fprintf(stdout, "[ERROR] %s: specified debug output to \"%s\" when forking\n",
				PACKAGE, cwdaemon_debug_f_path);

			exit(EXIT_FAILURE); // TODO (acerion) 2023.04.24: don't call exit(), return error instead.
		}
	}


	/* Handle valid situations (after eliminating possible invalid
	   situations above). */

	if (!cwdaemon_debug_f_path) {

		if (!forking) {
			/* stdout is (for historical reasons) a
			   default file for printing debug strings
			   when not forking, so if a debug file hasn't
			   been defined in command line, the
			   cwdaemon_debug_f_path will be NULL. Treat
			   this situation accordingly.*/

			fprintf(stderr, "debug output: stdout\n");
			/* This has been already done in main(), but
			   do it again to be super-explicit. */
			cwdaemon_debug_f = stdout;
		} else {
			/* Remember that in main() the file is set to
			   stdout. Obviously we aren't going to use
			   the stdout when daemon has forked. */
			cwdaemon_debug_f = NULL;
		}

	} else if (!strcmp(cwdaemon_debug_f_path, "stdout")) {

		/* We shouldn't get here when forking (invalid
		   situation handled by first "if" in the
		   function). */
		assert(!forking);

		fprintf(stderr, "debug output: stdout\n");
		cwdaemon_debug_f = stdout;

	} else if (!strcmp(cwdaemon_debug_f_path, "stderr")) {

		/* We shouldn't get here when forking (invalid
		   situation handled by first "if" in the
		   function). */
		assert(!forking);

		fprintf(stderr, "debug output: stderr\n");
		cwdaemon_debug_f = stderr;

	} else { /* cwdaemon_debug_f_path is a path to disc file. */

		/* We can get here when both forking and not forking. */

		fprintf(stderr, "debug output: %s\n", cwdaemon_debug_f_path);

		cwdaemon_debug_f = fopen(cwdaemon_debug_f_path, "w+");
		if (!cwdaemon_debug_f) {
			fprintf(stderr, "%s: failed to open output debug file \"%s\"\n", PACKAGE, cwdaemon_debug_f_path);
			exit(EXIT_FAILURE); // TODO (acerion): don't call exit(), return error instead.
		}
	}

	return;
}




void cwdaemon_debug_close(void)
{
	if (cwdaemon_debug_f
	    && cwdaemon_debug_f != stdout
	    && cwdaemon_debug_f != stderr) {

		fclose(cwdaemon_debug_f);
		cwdaemon_debug_f = NULL;
	}

	return;
}





