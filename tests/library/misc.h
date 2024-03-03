#ifndef CWDAEMON_TEST_LIB_MISC_H
#define CWDAEMON_TEST_LIB_MISC_H




#include <arpa/inet.h> /* in_port_t */
#include <sys/types.h> /* pid_t */
#include <time.h>




/** Data type used in handling exit of a child process. */
typedef struct child_exit_info_t {
	pid_t pid;                            /**< pid of process on which to do waitpid(). */
	struct timespec sigchld_timestamp;    /**< timestamp at which sigchld has occurred. */
	int wstatus;                          /**< Second arg to waitpid(). */
	pid_t waitpid_retv;                   /**< Value returned by waitpid(). */
} child_exit_info_t;




/**
   @brief Find a UDP port that is not used on local machine

   The port is semi-randomly selected from range of non-privileged ports
   (i.e. from range <1024 - 65535>, inclusive. See also
   CWDAEMON_NETWORK_PORT_MIN and CWDAEMON_NETWORK_PORT_MAX).

   The function is slightly biased towards returning cwdaemon's default port
   CWDAEMON_NETWORK_PORT_DEFAULT == 6789.

   Port is returned through @p port function argument.

   @param[out] port Selected port

   @return 0 on success
   @return -1 on failure to find an unused port
*/
int find_unused_random_biased_local_udp_port(in_port_t * port);




/**
   Get value of Morse code speed to be used by tests

   On errors, as a fallback, the function returns some sane default value.

   The value is slightly random.

   This function should be used to get a value of speed in tests that DO NOT
   test cwdaemon's "speed" parameter - in those tests we want to have some
   valid speed that perhaps varies between tests runs. For tests that do test
   the speed, you should use more specialized code.

   @return Value of Morse code speed to be used in a test
*/
int test_get_test_wpm(void);




/**
   Get value of tone (frequency) of sound to be used by tests

   On errors, as a fallback, the function returns some sane default value.

   The value is slightly random.

   This function should be used to get a value of tone in tests that DO NOT
   test cwdaemon's "tone" parameter - in those tests we want to have some
   valid tone that perhaps varies between tests runs. For tests that do test
   the tone, you should use more specialized code.

   @return Value of frequency of sound to be used in a test
*/
int test_get_test_tone(void);




#endif /* #ifndef CWDAEMON_TEST_LIB_MISC_H */

