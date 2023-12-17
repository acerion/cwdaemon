#ifndef CWDAEMON_TEST_LIB_PROCESS_H
#define CWDAEMON_TEST_LIB_PROCESS_H




#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/types.h> /* pid_t */

#include <libcw.h>

#include "client.h"




typedef struct cwdaemon_process_t {
	int fd;       /* Socket, on which the process will be reachable. */
	pid_t pid;    /* pid of cwdaemon process. */
	int l4_port;  /* Network port, on which cwdaemon has been started and is listening. */
} cwdaemon_process_t;




typedef struct {
	unsigned int pin_keying;  /**< tty port pin used for keying. */
	unsigned int pin_ptt;     /**< tty port pin used for PTT. */
} tty_pins_t;




typedef struct {
	char tone[10];
	enum cw_audio_systems sound_system;
	bool nofork;             /* -n, --nofork; don't fork. */
	char cwdevice_name[16];  /* Name of a device in /dev/. The name does not include "/dev/" */
	int wpm;
	tty_pins_t tty_pins; /**< Configuration of pins of tty port to be used as cwdevice by cwdaemon. */

	/* IP address of machine where cwdaemon is available, If empty, local
	   IP will be used. */
	char l3_address[INET6_ADDRSTRLEN];

	/*
	  Layer 4 port where cwdaemon is available. Passed to cwdaemon
	  through -p/--port command line arg.

	  negative value: use default cwdaemon port;
	  0: use random port;
	  positive value: use given port value;

	  I'm using zero to signify random port, because this should be the
	  default testing method: to run a cwdaemon with default port, and
	  zero is the easiest value to assign to this field.
	*/
	int l4_port;
} cwdaemon_opts_t;




/**
   @return 0 on success
   @return -1 on failure
*/
int cwdaemon_start_and_connect(const cwdaemon_opts_t * opts, cwdaemon_process_t * cwdaemon, client_t * client);




/**
   @brief Terminate a process after 'delay_ms' milliseconds

   First try to terminate a process by sending to it EXIT request, and if
   this doesn't work, send a KILL signal.

   The EXIT request is sent after @p delay_ms milliseconds.

   This function is non-blocking.
*/
void cwdaemon_process_do_delayed_termination(cwdaemon_process_t * cwdaemon, int delay_ms);




/**
   Wait for end of cwdaemon to exit. The exit should have been requested by
   cwdaemon_process_do_delayed_termination().

   @return 0 if process exited cleanly as asked
   @return -1 if process didn't exit cleanly and was killed by cwdaemon_process_do_delayed_termination().
*/
int cwdaemon_process_wait_for_exit(cwdaemon_process_t * cwdaemon);




/**
   @brief Stop the local test instance of cwdaemon

   @param cwdaemon Local server to be stopped
   @param client Client instance to use to communicate with the server

   @return 0 on success
   @return -1 on failure
*/
int local_server_stop(cwdaemon_process_t * cwdaemon, client_t * client);




#endif /* #ifndef CWDAEMON_TEST_LIB_PROCESS_H */


