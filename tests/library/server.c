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
 * GNU Library General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   Data types and functions for managing a child process.

   Functional tests may need to start an instance of cwdaemon server process
   on which to run tests. The code in this file makes it easier to start,
   control and stop the process.
*/




#define _POSIX_C_SOURCE 200809L /* kill() */

#include "config.h"

/* For kill() on FreeBSD 13.2 */
#include <signal.h>
#include <sys/types.h>

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h> /* getenv() */
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"
#include "misc.h"
#include "random.h"
#include "server.h"
#include "sleep.h"
#include "socket.h"
#include "src/cwdaemon.h"
#include "supervisor.h"




/* Count of non-NULL items to be put in env[] passed to execve(). */
#define ENV_MAX_COUNT   2




static int cwdaemon_start(const char * path, const cwdaemon_opts_t * opts, server_t * server);
static int prepare_env(char * env[ENV_MAX_COUNT + 1]);

static char g_arg_tone[TONE_BUF_SIZE] = { 0 };



// LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/acerion/lib ~/sbin/cwdaemon -d ttyS0 -n -x p  -s 10 -T 1000 > /dev/null
/* TODO: make sure that child process is killed when a test is terminated
   with Ctrl+C. */




/**
   @brief Return port number per specification in @p opts

   The returned value will either be an explicity value specified in @p opts,
   or a random value if @p opts doesn't specify it.

   The random value is slightly biased towards being a cwdaemon's default
   value of port.

   Function uses 'int' instead of 'in_port_t' type for port value because in
   some tests we may explicitly specify an invalid port value (e.g. 100000)
   in @p opts to see how cwdaemon handles it. 'in_port_t' type would not
   allow such value.

   @param[in] opts cwdaemon's configuration options
   @paran[out] port Port on which cwdaemon should listen

   @return -1 on errors
   @return 0 on success
*/
static int get_port_number(const cwdaemon_opts_t * opts, int * port)
{
	if (opts->l4_port == -1) {
		/* Special case used in "option_port" functional test. Run the
		   process with invalid port zero. */
		test_log_warn("Test: requested value of port is out of range: %d, continuing with the value anyway\n", 0);
		*port = 0;
	} else if (opts->l4_port == 0) {
		/* Generate random (but still valid, within valid range) port
		   number. */
		in_port_t random_valid_port = 0;
		if (0 != find_unused_random_biased_local_udp_port(&random_valid_port)) {
			test_log_err("Test: failed to get random port %s\n", "");
			return -1;
		}
		*port = (int) random_valid_port;
	} else {
		if (opts->l4_port < CWDAEMON_NETWORK_PORT_MIN || opts->l4_port > CWDAEMON_NETWORK_PORT_MAX) {
			/* Invalid (out of range) values may be allowed in code testing
			   how cwdaemon process handles invalid values of ports.
			   Therefore this is just a warning situation. */
			test_log_warn("Test: requested value of port is out of range: %d, continuing with the value anyway\n", opts->l4_port);
		}
		*port = opts->l4_port;
	}

	return 0;
}




/**
   @brief Conditionally add "network port" option to @p argv

   The option is added either in short form "-p" or in long form "--port".
   The form is selected at random. This is used to test that both short
   option ("-p") and long option ("--port") are handled correctly by
   cwdaemon.

   If currently selected port number is cwdaemon's default port number, the
   function may at random decide not to add the "network port" option to @p
   argv. This is used to test that cwdaemon can run correctly without
   explicit port option. cwdaemon should use default port in such situation.

   @param[in] opts cwdaemon's configuration options
   @param[out] argv processes' argv vector to which to append "port" option
   @param[in/out] argc Index to @p argv, incremented by this function
   @param[out] port Port selected by this function based on config from @p opts

   @return 0 on success
   @return -1 on failure
*/
static int get_option_port(const cwdaemon_opts_t * opts, const char ** argv, int * argc, int * port)
{
	if (0 != get_port_number(opts, port)) {
		test_log_err("Test: failed to get port number from opts %s\n", "");
		return -1;
	}

	if (*port == CWDAEMON_NETWORK_PORT_DEFAULT) {
		/* We can, but we don't have to add explicit "port" command line
		   option when a default port value is to be used by the server. */
		bool explicit_port_argument = true;
		cwdaemon_random_bool(&explicit_port_argument);

		if (!explicit_port_argument) {
			/* Let cwdaemon start without explicitly specified port. */
			test_log_info("Test: cwdaemon will start with default port, without explicit 'port' option %s\n", "");
			return 0;
		}
	}

	bool use_long_opt = false;
	if (0 != cwdaemon_random_bool(&use_long_opt)) {
		test_log_err("Test: failed go get 'use long opt' random boolean %s\n", "");
		return -1;
	}

	static char port_buf[PORT_BUF_SIZE] = { 0 }; /* This must be static because the string will be shared with caller. */
	snprintf(port_buf, sizeof (port_buf), "%d", *port);
	if (use_long_opt) {
		argv[(*argc)++] = "--port";
	} else {
		argv[(*argc)++] = "-p";
	}
	argv[(*argc)++] = port_buf;

	return 0;
}




int cwdaemon_start(const char * cwdaemon_path, const cwdaemon_opts_t * opts, server_t * server)
{
	int l4_port = 0;

	const char * argv[40] = { 0 };
	int argc = 0;
	const char * top_level_exec_path = cwdaemon_path; /* First item in argv[] passed to execv(). */

	switch (opts->supervisor_id) {
	case supervisor_id_none:
		break;
	case supervisor_id_valgrind:
		get_args_valgrind(argv, &argc);
		top_level_exec_path = "/usr/bin/valgrind";
		break;
	case supervisor_id_gdb:
		get_args_gdb(argv, &argc);
		top_level_exec_path = "/usr/bin/gdb";
		break;
	}

	argv[argc++] = cwdaemon_path;

	if (0 != get_option_port(opts, argv, &argc, &l4_port)) {
		test_log_err("Test: failed to get 'port' option for command line %s\n", "");
		return -1;
	}

	pid_t pid = fork();
	if (0 == pid) {
		char * env[ENV_MAX_COUNT + 1] = { 0 };
		if (0 != prepare_env(env)) {
			test_log_err("Test: failed to prepare env table for child process %s\n", "");
			return -1;
		}

		if (0 != opts->tone) {
			argv[argc++] = "-T";
			snprintf(g_arg_tone, sizeof (g_arg_tone), "%d", opts->tone);
			argv[argc++] = g_arg_tone;
		}

		switch (opts->sound_system) {
		case CW_AUDIO_CONSOLE:
			argv[argc++] = "-x";
			argv[argc++] = "c";
			break;
		case CW_AUDIO_OSS:
			argv[argc++] = "-x";
			argv[argc++] = "o";
			break;
		case CW_AUDIO_ALSA:
			argv[argc++] = "-x";
			argv[argc++] = "a";
			break;
		case CW_AUDIO_PA:
			argv[argc++] = "-x";
			argv[argc++] = "p";
			break;
		case CW_AUDIO_SOUNDCARD:
			argv[argc++] = "-x";
			argv[argc++] = "s";
			break;
		case CW_AUDIO_NULL: /* It's not NONE, it's really NULL! */
			argv[argc++] = "-x";
			argv[argc++] = "n";
			break;
		case CW_AUDIO_NONE:
			; /* NOOP. NONE == 0. Just don't pass audio system arg to cwdaemon. */
			break;
		default:
			test_log_err("Test: unsupported %d sound system\n", opts->sound_system);
			return -1;
		};

		if (opts->nofork) {
			argv[argc++] = "-n";
		}
		if ('\0' != opts->cwdevice_name[0]) {
			argv[argc++] = "-d";
			argv[argc++] = opts->cwdevice_name;
		}
		char wpm_buf[WPM_BUF_SIZE] = { 0 };
		if (opts->wpm) {
			snprintf(wpm_buf, sizeof (wpm_buf), "%d", opts->wpm);
			argv[argc++] = "-s";
			argv[argc++] = wpm_buf;
		}
		if (opts->tty_pins.explicit) {
			/* tty options for cwdaemon server have been explicitly defined.
			   Use them here. */
			switch (opts->tty_pins.pin_keying) {
			case TIOCM_DTR:
				argv[argc++] = "-o";
				argv[argc++] = "key=dtr";
				break;
			case TIOCM_RTS:
				argv[argc++] = "-o";
				argv[argc++] = "key=rts";
				break;
			default:
				break;
			}

			switch (opts->tty_pins.pin_ptt) {
			case TIOCM_DTR:
				argv[argc++] = "-o";
				argv[argc++] = "ptt=dtr";
				break;
			case TIOCM_RTS:
				argv[argc++] = "-o";
				argv[argc++] = "ptt=rts";
				break;
			default:
				break;
			}
		} else {
			; /* Allow cwdaemon server to use default assignment of tty pins. */
		}


		int b = 0;
		while (argv[b]) {
			fprintf(stderr, "%s ", argv[b]);
			b++;
		}
		fprintf(stderr, "\n");

		execve(top_level_exec_path, (char * const *) argv, env);
		test_log_err("Test: returning after failed exec(): %s\n", strerror(errno));
		exit(EXIT_FAILURE); /* Calling "return -1" doesn't result in proper behaviour of waitpid. */
	} else {
		server->supervisor_id = opts->supervisor_id;

		/*
		  Wait for start of cwdaemon for two reasons:

		  1. if execve() fails, waitpid() will be able to detect this only
		     after some small delay (~30 ms). Calling waitpid(WNOHANG) too
		     early will result in detecting a running child process.
		  2. Give the cwdaemon some time to properly start.

		  More info on the second reason: I noticed that a Morse-receiver
		  test that was started immediately after start of cwdaemon was
		  always receiving first letter incorrectly. With 60 milliseconds the
		  behaviour was correct, but I'm setting it to 300 ms to be sure.

		  Usually the tests are written in a way to expect and discard errors
		  at the beginning of received text, but why add another factor that
		  decreases quality of receiving?

		  I haven't observed this in practice, but I think that when program
		  is ran under supervisor, it may take longer for it to start and be
		  responsive to test suite, compared to no supervisor. TODO acerion
		  2024.02.19: check this somehow.
		*/
		unsigned int milli_sleep_duration = 300;
		switch (server->supervisor_id) {
		case supervisor_id_none:
			milli_sleep_duration = 300;
			break;
		case supervisor_id_valgrind:
			milli_sleep_duration = 3000;
			break;
		case supervisor_id_gdb:
			milli_sleep_duration = 1000;
			break;
		}
		const int sleep_retv = test_millisleep_nonintr(milli_sleep_duration);
		if (sleep_retv) {
			test_log_err("Test: error during sleep in parent: %s\n", strerror(errno));
			return -1;
		}

		pid_t waited_pid = waitpid(pid, &server->wstatus, WNOHANG);
		if (pid == waited_pid) {
			test_log_notice("Test: Child process %d changed state\n", pid);
			if (WIFEXITED(server->wstatus)) {
				test_log_err("Test: child process exited too early, exit status = %d\n", WEXITSTATUS(server->wstatus));
			} else if (WIFSIGNALED(server->wstatus)) {
				test_log_err("Test: child process was terminated by signal %d\n", WTERMSIG(server->wstatus));
			} else if (WIFSTOPPED(server->wstatus)) {
				test_log_err("Test: child process was stopped by signal %d\n", WSTOPSIG(server->wstatus));
			} else {
				test_log_err("Test: child process didn't start correctly due to unknown reason %s\n", "");
			}
			return -1;
		} else if (0 == waited_pid) {
			test_log_info("Test: cwdaemon server started, pid = %d, l4 port = %d\n", pid, l4_port);
			server->pid = pid;
			server->l4_port = l4_port;
			return 0;
		} else {
			/* Some test cases may expect the server not to start (e.g. when
			   testing values of options out-of-range. Therefore this is just
			   a warning. */
			test_log_warn("Test: starting of process: waitpid() returns %d, errno = %s\n", waited_pid, strerror(errno));
			return -1;
		}
	}
}




int server_start(const cwdaemon_opts_t * opts, server_t * server)
{
	const char * path = TEST_CWDAEMON_PATH;
	if (0 != cwdaemon_start(path, opts, server)) {
		/* Some test cases may expect the server not to start (e.g. when
		   testing values of options out-of-range. Therefore this is just a
		   warning. */

		/* TODO (acerion) 2024.02.09: get more info about why process failed.
		   Maybe previous test run crashed and old instance of cwdaemon is
		   already running and listening on given port? */
		test_log_warn("Test: failed to start cwdaemon server %s\n", "");
		return -1;
	}

	if (0 == strlen(opts->l3_address)) {
		snprintf(server->ip_address, sizeof (server->ip_address), "%s", "127.0.0.1");
	} else {
		snprintf(server->ip_address, sizeof (server->ip_address), "%s", opts->l3_address);
	}

	return 0;
}



/**
   Prepare environment of cwdaemon process started with execve().

   @return 0 on success
   @return -1 on failure
*/
static int prepare_env(char * env[ENV_MAX_COUNT + 1])
{
#define BUF_SIZE 1024

	int env_i = 0;
	if (1) {
		/*
		  During development phase the libcw library may be located
		  elsewhere, so point the program to that location.
		*/
		static char ld_path[BUF_SIZE] = { 0 };
		int n = snprintf(ld_path, sizeof (ld_path), "%s", "LD_LIBRARY_PATH=$LD_LIBRARY_PATH:" LIBCW_LIBDIR "/");
		if (n >= BUF_SIZE) {
			test_log_err("Test: overflow when writing ld_path to env table %s\n", "");
			return -1;
		}
		env[env_i] = ld_path;
		env_i++;
	}


	if (1) {
		/*
		  Passing this env to cwdaemon is necessary if the cwdaemon
		  will be connecting to PulseAudio server.

		  On Linux Mint 20.3 an attempt to start cwdaemon as child
		  process of test binary (through execve()), and make it
		  connect to PulseAudio server, resulted in "Connection
		  refused" error.

		  cwdaemon started directly from command line, or with
		  something other than execve(), has access to full env
		  table, including XDG_RUNTIME_DIR and the problem doesn't
		  occur.
		*/
		static char xdg_runtime[BUF_SIZE] = { 0 };
		const char * name = "XDG_RUNTIME_DIR";
		const char * value = getenv("XDG_RUNTIME_DIR");
		if (value) {
			int n = snprintf(xdg_runtime, sizeof (xdg_runtime), "%s=%s", name, value);
			if (n >= BUF_SIZE) {
				test_log_err("Test: overflow when writing XDG RUNTIME DIR to env table %s\n", "");
				return -1;
			}
			env[env_i] = xdg_runtime;
			env_i++;
		}
	}

	if (env_i > ENV_MAX_COUNT) {
		/* TODO: it may be too late for this check: writing past the array size may have already happened. */
		test_log_err("Test: count of env items in env table is exceeded: %d > %d\n", env_i, ENV_MAX_COUNT);
		return -1;
	}

	env[env_i] = NULL;
	return 0;
#undef BUF_SIZE
}




int local_server_stop(server_t * server, client_t * client)
{
	int retval = 0;

	/*
	  TODO acerion 2023.12.17: first check if the process with given pid and
	  process name exists. It's possible that a test has crashed a process.
	  If it happened, we have to know about it and react to it.
	*/

	/* First ask nicely for a clean exit. */
	if (0 != client_send_esc_request(client, CWDAEMON_ESC_REQUEST_EXIT, "", 0)) {
		test_log_err("cwdaemon server: failed to send EXIT request to server %s\n", "");
	}

	/* Give the server some time to exit. */
	const int sleep_retv = test_sleep_nonintr(2);
	if (sleep_retv) {
		test_log_notice("Test: error during sleep while waiting for local server to exit %s\n", "");
	}

	if (server->supervisor_id != supervisor_id_none) {
		test_log_info("cwdaemon server: server was started inside of supervisor (e.g. valgrind or gdb). Not doing anything beyond sending EXIT request %s\n", "");
		/* Return now. Don't try to send SIGKILL to process with server->pid,
		   because that's the PID of supervisor. */
		return 0;
	}

	/* Now check if test instance of cwdaemon is no longer present, as expected. */
	if (0 == waitpid(server->pid, &server->wstatus, WNOHANG)) {
		/* Process still exists, kill it. */
		test_log_err("Test: local test instance of cwdaemon process is still active despite being asked to exit, sending SIGKILL %s\n", "");
		/* The fact that we need to kill cwdaemon with a
		   signal is a bug. */
		kill(server->pid, SIGKILL);
		test_log_notice("Test: local test instance of cwdaemon was forcibly killed %s\n", "");
		retval = -1;
	}

	return retval;
}

