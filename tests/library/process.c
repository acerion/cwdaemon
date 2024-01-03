/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
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

#include "misc.h"
#include "process.h"
#include "socket.h"
#include "src/lib/sleep.h"




/* Count of non-NULL items to be put in env[] passed to execve(). */
#define ENV_MAX_COUNT   2




static int cwdaemon_start(const char * path, const cwdaemon_opts_t * opts, cwdaemon_server_t * cwdaemon);
static int prepare_env(char * env[ENV_MAX_COUNT + 1]);

static char g_arg_tone[10] = { 0 };



// LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/acerion/lib ~/sbin/cwdaemon -d ttyS0 -n -x p  -s 10 -T 1000 > /dev/null
/* TODO: make sure that child process is killed when a test is terminated
   with Ctrl+C. */




int cwdaemon_start(const char * path, const cwdaemon_opts_t * opts, cwdaemon_server_t * cwdaemon)
{
	const int default_l4_port = 6789; /* TODO: replace with a constant from cwdaemon. */
	int l4_port = 0;
	if (opts->l4_port < 0) {
		l4_port = default_l4_port;
	} else if (opts->l4_port == 0) {
		int random_port = find_unused_random_local_udp_port();
		if (random_port > 0) {
			l4_port = random_port;
		} else {
			l4_port = default_l4_port; /* Too bad, use a default port. Not the end of the world. */
		}
	} else {
		if (opts->l4_port >= 1024 && opts->l4_port <= 65535) {
			l4_port = opts->l4_port;
		} else {
			fprintf(stderr, "[EE] invalid L4 port value %d\n", opts->l4_port);
			return -1;
		}
	}

	pid_t pid = fork();
	if (0 == pid) {
		char * env[ENV_MAX_COUNT + 1] = { 0 };
		if (0 != prepare_env(env)) {
			fprintf(stderr, "[EE] failed to prepare env table for child process\n");
			return -1;
		}

		const char * argv[20] = { 0 };
		int a = 0;
		argv[a++] = path;
		if (0 != opts->tone) {
			argv[a++] = "-T";
			snprintf(g_arg_tone, sizeof (g_arg_tone), "%d", opts->tone);
			argv[a++] = g_arg_tone;
		}

		switch (opts->sound_system) {
		case CW_AUDIO_CONSOLE:
			argv[a++] = "-x";
			argv[a++] = "c";
			break;
		case CW_AUDIO_OSS:
			argv[a++] = "-x";
			argv[a++] = "o";
			break;
		case CW_AUDIO_ALSA:
			argv[a++] = "-x";
			argv[a++] = "a";
			break;
		case CW_AUDIO_PA:
			argv[a++] = "-x";
			argv[a++] = "p";
			break;
		case CW_AUDIO_SOUNDCARD:
			argv[a++] = "-x";
			argv[a++] = "s";
			break;
		case CW_AUDIO_NULL: /* It's not NONE, it's really NULL! */
			argv[a++] = "-x";
			argv[a++] = "n";
			break;
		case CW_AUDIO_NONE:
			; /* NOOP. NONE == 0. Just don't pass audio system arg to cwdaemon. */
			break;
		default:
			fprintf(stderr, "[EE] unsupported %d sound system\n", opts->sound_system);
			return -1;
		};

		if (opts->nofork) {
			argv[a++] = "-n";
		}
		if ('\0' != opts->cwdevice_name[0]) {
			argv[a++] = "-d";
			argv[a++] = opts->cwdevice_name;
		}
		char wpm_buf[16] = { 0 };
		if (opts->wpm) {
			snprintf(wpm_buf, sizeof (wpm_buf), "%d", opts->wpm);
			argv[a++] = "-s";
			argv[a++] = wpm_buf;
		}
		if (opts->tty_pins.explicit) {
			/* tty options for cwdaemon server have been explicitly defined.
			   Use them here. */
			switch (opts->tty_pins.pin_keying) {
			case TIOCM_DTR:
				argv[a++] = "-o";
				argv[a++] = "key=dtr";
				break;
			case TIOCM_RTS:
				argv[a++] = "-o";
				argv[a++] = "key=rts";
				break;
			default:
				break;
			}

			switch (opts->tty_pins.pin_ptt) {
			case TIOCM_DTR:
				argv[a++] = "-o";
				argv[a++] = "ptt=dtr";
				break;
			case TIOCM_RTS:
				argv[a++] = "-o";
				argv[a++] = "ptt=rts";
				break;
			default:
				break;
			}
		} else {
			; /* Allow cwdaemon server to use default assignment of tty pins. */
		}


		char port_buf[16] = { 0 };
		snprintf(port_buf, sizeof (port_buf), "%d", l4_port);
		argv[a++] = "-p";
		argv[a++] = port_buf;

		int b = 0;
		while (argv[b]) {
			fprintf(stderr, "%s ", argv[b]);
			b++;
		}
		fprintf(stderr, "\n");

		execve(path, (char * const *) argv, env);
		fprintf(stderr, "[EE] Returning after failed exec(): %s\n", strerror(errno));
		exit(EXIT_FAILURE); /* Calling "return -1" doesn't result in proper behaviour of waitpid. */
	} else {
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
		*/
		const int sleep_retv = millisleep_nonintr(300);
		if (sleep_retv) {
			fprintf(stderr, "[EE] Error during sleep in parent: %s\n", strerror(errno));
			return -1;
		}

		pid_t waited_pid = waitpid(pid, &cwdaemon->wstatus, WNOHANG);
		if (pid == waited_pid) {
			fprintf(stderr, "[NN] Child process %d changed state\n", pid);
			if (WIFEXITED(cwdaemon->wstatus)) {
				fprintf(stderr, "[EE] Child process exited too early, exit status = %d\n", WEXITSTATUS(cwdaemon->wstatus));
			} else if (WIFSIGNALED(cwdaemon->wstatus)) {
				fprintf(stderr, "[EE] Child process was terminated by signal %d\n", WTERMSIG(cwdaemon->wstatus));
			} else if (WIFSTOPPED(cwdaemon->wstatus)) {
				fprintf(stderr, "[EE] Child process was stopped by signal %d\n", WSTOPSIG(cwdaemon->wstatus));
			} else {
				fprintf(stderr, "[EE] Child process didn't start correctly due to unknown reason\n");
			}
			return -1;
		} else if (0 == waited_pid) {
			fprintf(stderr, "[II] cwdaemon started, pid = %d, l4 port = %d\n", pid, l4_port);
			cwdaemon->pid = pid;
			cwdaemon->l4_port = l4_port;
			return 0;
		} else {
			fprintf(stderr, "[EE] waitpid() returns %d\n", waited_pid);
			return -1;
		}
	}
}




int cwdaemon_start_and_connect(const cwdaemon_opts_t * opts, cwdaemon_server_t * cwdaemon, client_t * client)
{
	const char * path = TEST_CWDAEMON_PATH;
	if (0 != cwdaemon_start(path, opts, cwdaemon)) {
		fprintf(stderr, "[EE] Failed to start cwdaemon\n");
		return -1;
	}

	char cwdaemon_address[INET6_ADDRSTRLEN] = { 0 };
	if (0 == strlen(opts->l3_address)) {
		snprintf(cwdaemon_address, sizeof (cwdaemon_address), "%s", "127.0.0.1");
	} else {
		snprintf(cwdaemon_address, sizeof (cwdaemon_address), "%s", opts->l3_address);
	}
	char cwdaemon_port[16] = { 0 };
	snprintf(cwdaemon_port, sizeof (cwdaemon_port), "%d", cwdaemon->l4_port);

	client->sock = cwdaemon_socket_connect(cwdaemon_address, cwdaemon_port);
	if (client->sock < 0) {
		fprintf(stderr, "[EE] Failed to connect to cwdaemon socket\n");
		return -1;
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
			fprintf(stderr, "[EE] overflow when writing ld_path to env table\n");
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
				fprintf(stderr, "[EE] overflow when writing XDG RUNTIME DIR to env table\n");
				return -1;
			}
			env[env_i] = xdg_runtime;
			env_i++;
		}
	}

	if (env_i > ENV_MAX_COUNT) {
		/* TODO: it may be too late for this check: writing past the array size may have already happened. */
		fprintf(stderr, "[EE] Count of env items in env table is exceeded: %d > %d\n", env_i, ENV_MAX_COUNT);
		return -1;
	}

	env[env_i] = NULL;
	return 0;
#undef BUF_SIZE
}




int local_server_stop(cwdaemon_server_t * server, client_t * client)
{
	int retval = 0;

	/*
	  TODO acerion 2023.12.17: first check if the process with given pid and
	  process name exists. It's possible that a test has crashed a process.
	  If it happened, we have to know about it and react to it.
	*/

	/* First ask nicely for a clean exit. */
	client_send_request(client, CWDAEMON_REQUEST_EXIT, "");

	/* Give the server some time to exit. */
	const int sleep_retv = sleep_nonintr(2);
	if (sleep_retv) {
		fprintf(stderr, "[NOTIC] error during sleep while waiting for local server to exit\n");
	}

	/* Now check if test instance of cwdaemon is no longer present, as expected. */
	if (0 == waitpid(server->pid, &server->wstatus, WNOHANG)) {
		/* Process still exists, kill it. */
		fprintf(stderr, "[ERROR] Local test instance of cwdaemon process is still active despite being asked to exit, sending SIGKILL\n");
		/* The fact that we need to kill cwdaemon with a
		   signal is a bug. */
		kill(server->pid, SIGKILL);
		fprintf(stderr, "[NOTIC] Local test instance of cwdaemon was forcibly killed\n");
		retval = -1;
	}

	return retval;
}

