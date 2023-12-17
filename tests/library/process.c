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




static int cwdaemon_start(const char * path, const cwdaemon_opts_t * opts, cwdaemon_process_t * cwdaemon);
static void * terminate_cwdaemon_fn(void * cwdaemon_arg);
static int prepare_env(char * env[ENV_MAX_COUNT + 1]);




// LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/acerion/lib ~/sbin/cwdaemon -d ttyS0 -n -x p  -s 10 -T 1000 > /dev/null
/* TODO: make sure that child process is killed when a test is terminated
   with Ctrl+C. */




int cwdaemon_start(const char * path, const cwdaemon_opts_t * opts, cwdaemon_process_t * cwdaemon)
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
		if ('\0' != opts->tone[0]) {
			argv[a++] = "-T";
			argv[a++] = opts->tone;
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
		return -1;
	} else {
		/*
		  300 milliseconds. Give the process some time to start.
		  Delay introduced after I noticed that a receiver test that
		  was started immediately after start of cwdaemon was always
		  receiving first letter incorrectly. With 60 milliseconds
		  the behaviour was correct, but I'm setting it to 300 ms to
		  be sure.

		  Usually the tests are written in a way to expect and
		  discard errors at the beginning of received text, but why
		  add another factor that decreases quality of receiving?
		*/
		const int sleep_retv = millisleep_nonintr(300);
		if (sleep_retv) {
			fprintf(stderr, "[ERROR] error during sleep in parent\n");
		}

		fprintf(stderr, "[II] cwdaemon started, pid = %d, l4 port = %d\n", pid, l4_port);

		cwdaemon->pid = pid;
		cwdaemon->l4_port = l4_port;
		return 0;
	}
}




void cwdaemon_process_do_delayed_termination(cwdaemon_process_t * cwdaemon, int delay_ms)
{
	const int sleep_retv = millisleep_nonintr(delay_ms);
	if (sleep_retv) {
		fprintf(stderr, "[ERROR] error in sleep in delayed termination\n");
	}
	static pthread_t thread_id;
	pthread_create(&thread_id, NULL, terminate_cwdaemon_fn, cwdaemon);
}




static void * terminate_cwdaemon_fn(void * cwdaemon_arg)
{
	cwdaemon_process_t * cwdaemon = (cwdaemon_process_t *) cwdaemon_arg;

	/* First ask nicely for a clean exit. */
	cwdaemon_socket_send_request(cwdaemon->fd, CWDAEMON_REQUEST_EXIT, "");

	/* Give cwdaemon some time to exit cleanly. */
	const int sleep_retv = sleep_nonintr(2);
	if (sleep_retv) {
		fprintf(stderr, "[ERROR] error in sleep in termination\n");
	}

	int wstatus = 0;
	if (0 == waitpid(cwdaemon->pid, &wstatus, WNOHANG)) {
		/* Process still exists, kill it. */
		fprintf(stderr, "[WW] Child cwdaemon process is still active despite being asked to exit, sending SIGKILL\n");
		/* The fact that we need to kill cwdaemon with a signal is a
		   bug. It will be detected by a test executable when the
		   executable calls wait() on child pid. */
		kill(cwdaemon->pid, SIGKILL);
	}
	return NULL;
}




int cwdaemon_process_wait_for_exit(cwdaemon_process_t * cwdaemon)
{
	int wstatus = 0;
	pid_t waited_pid = waitpid(cwdaemon->pid, &wstatus, 0);
	if (waited_pid != cwdaemon->pid) {
		fprintf(stderr, "[EE] waitpid() returns %d after waiting for child %d\n", waited_pid, cwdaemon->pid);
		return -1;
	}
	if (!! WIFEXITED(wstatus)) {
		fprintf(stderr, "[II] Child cwdaemon process exited cleanly\n");
		return 0;
	} else {
		const int signaled = WIFSIGNALED(wstatus);
		fprintf(stderr, "[EE] Child cwdaemon process didn't exit cleanly: WIFSIGNALED = %d\n", signaled);
		if (signaled) {
			fprintf(stderr, "[EE] Child cwdaemon process received signal %d\n", WTERMSIG(wstatus));
		}
		return -1;
	}
}




int cwdaemon_start_and_connect(const cwdaemon_opts_t * opts, cwdaemon_process_t * cwdaemon)
{
	const char * path = ROOT_DIR "/src/cwdaemon";
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

	cwdaemon->fd = cwdaemon_socket_connect(cwdaemon_address, cwdaemon_port);
	if (cwdaemon->fd < 0) {
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

