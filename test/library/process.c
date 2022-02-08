#define _DEFAULT_SOURCE /* usleep() */
#define _POSIX_C_SOURCE /* kill() */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>




#include "process.h"
#include "socket.h"




static void * terminate_cwdaemon_fn(void * child_arg);




/* TODO: make sure that child process is killed when a test is terminated
   with Ctrl+C. */




pid_t cwdaemon_start(const char * path, const cwdaemon_opts_t * opts)
{
	pid_t pid = fork();
	if (0 == pid) {
		char * const env[] = { "LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/acerion/lib", NULL };

		const char * argv[20] = { 0 };
		int a = 0;
		argv[a++] = path;
		if ('\0' != opts->tone[0]) {
			argv[a++] = "-T";
			argv[a++] = opts->tone;
		}
		if ('\0' != opts->sound_system[0]) {
			argv[a++] = "-x";
			argv[a++] = opts->sound_system;
		}
		if ('\0' != opts->nofork[0]) {
			argv[a++] = "-n";
		}
		if ('\0' != opts->cwdevice[0]) {
			argv[a++] = "-d";
			argv[a++] = opts->cwdevice;
		}
		if ('\0' != opts->wpm[0]) {
			argv[a++] = "-s";
			argv[a++] = opts->wpm;
		}

		execve(path, (char * const *) argv, env);
		fprintf(stderr, "[EE] Returning after failed exec(): %s\n", strerror(errno));
		return 0;
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
		usleep(60 * 1000);

		fprintf(stderr, "[II] cwdaemon started, pid = %d\n", pid);
		return pid;
	}
}




void cwdaemon_process_do_delayed_termination(cwdaemon_process_t * child, int delay_ms)
{
	usleep(1000 * delay_ms);
	static pthread_t thread_id;
	pthread_create(&thread_id, NULL, terminate_cwdaemon_fn, child);
}




static void * terminate_cwdaemon_fn(void * child_arg)
{
	cwdaemon_process_t * child = (cwdaemon_process_t *) child_arg;

	/* First ask nicely for a clean exit. */
	cwdaemon_socket_send_request(child->fd, CWDAEMON_REQUEST_EXIT, "");

	/* Give cwdaemon some time to exit cleanly. */
	sleep(2);

	int wstatus = 0;
	if (0 == waitpid(child->pid, &wstatus, WNOHANG)) {
		/* Process still exists, kill it. */
		fprintf(stderr, "[WW] Child cwdaemon process is still active despite being asked to exit, sending SIGKILL\n");
		/* The fact that we need to kill cwdaemon with a signal is a
		   bug. It will be detected by a test executable when the
		   executable calls wait() on child pid. */
		kill(child->pid, SIGKILL);
	}
	return NULL;
}




int cwdaemon_process_wait_for_exit(cwdaemon_process_t * child)
{
	int wstatus = 0;
	pid_t waited_child = wait(&wstatus);
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


