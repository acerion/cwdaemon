#ifndef CWDAEMON_TEST_LIB_PROCESS_H
#define CWDAEMON_TEST_LIB_PROCESS_H




#include <stdbool.h>
#include <sys/types.h> /* pid_t */




typedef struct cwdaemon_process_t {
	int fd;       /* Socket, on which the process will be reachable. */
	pid_t pid;    /* pid of cwdaemon process. */
	int l4_port;  /* Network port, on which cwdaemon will be listening. */
} cwdaemon_process_t;




typedef struct {
	char tone[10];
	char sound_system[5];
	bool nofork;             /* -n, --nofork; don't fork. */
	char cwdevice[16];
	char wpm[10];
	bool use_random_l4_port; /* -p, --port; if false, a default cwdaemon port will be used. */
} cwdaemon_opts_t;




/**
   @return 0 on success
   @return -1 on failure
*/
int cwdaemon_start(const char * path, const cwdaemon_opts_t * opts, cwdaemon_process_t * child);





/**
   @brief Terminate a process after 'delay_ms' milliseconds

   First try to terminate a process by sending to it EXIT request, and if
   this doesn't work, send a KILL signal.

   The EXIT request is sent after @p delay_ms milliseconds.

   This function is non-blocking.
*/
void cwdaemon_process_do_delayed_termination(cwdaemon_process_t * child, int delay_ms);




/**
   Wait for end of cwdaemon to exit. The exit should have been requested by
   cwdaemon_process_do_delayed_termination().

   @return 0 if process exited cleanly as asked
   @return -1 if process didn't exit cleanly and was killed by cwdaemon_process_do_delayed_termination().
*/
int cwdaemon_process_wait_for_exit(cwdaemon_process_t * child);




#endif /* #ifndef CWDAEMON_TEST_LIB_PROCESS_H */


