#ifndef CWDAEMON_TEST_LIB_PROCESS_H
#define CWDAEMON_TEST_LIB_PROCESS_H




#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/types.h> /* pid_t */

#include <libcw.h>

#include "client.h"




/* For now this structure doesn't allow for usage and tests of remote
   cwdaemon server. */
typedef struct cwdaemon_process_t {
	pid_t pid;    /**< pid of local test instance of cwdaemon process. */
	int l4_port;  /**< Network port, on which cwdaemon server is available and listening. */
} cwdaemon_process_t;




/*
  Structure describing pins of tty cwdevice.

  You can assign TIOCM_RTS and TIOCM_DTR values to these pins.
*/
typedef struct {
	unsigned int pin_keying;  /**< Pin of tty port that is used for keying (sending dots and dashes). */
	unsigned int pin_ptt;     /**< Pin of tty port that is used for PTT. */
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
   @brief Stop the local test instance of cwdaemon

   @param server Local server to be stopped
   @param client Client instance to use to communicate with the server

   @return 0 on success
   @return -1 on failure
*/
int local_server_stop(cwdaemon_process_t * server, client_t * client);




#endif /* #ifndef CWDAEMON_TEST_LIB_PROCESS_H */


