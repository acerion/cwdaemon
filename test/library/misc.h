#ifndef CWDAEMON_TEST_LIB_MISC_H
#define CWDAEMON_TEST_LIB_MISC_H




#include "process.h"
#include "cw_rec_utils.h"




typedef struct {
	int wpm;
} helpers_opts_t;





/**
   @brief Ask cwdaemon to play a text message, receive it on serial line console

   @param[in] cwdaemon cwdaemon child process used for the test
   @param[in] messsage_value text to played

   @return 0 if text was received successfully
   @return -1 otherwise
*/
int cwdaemon_play_text_and_receive(cwdaemon_process_t * cwdaemon, const char * message_value);




/**
   @brief Find a UDP port that is not used on local machine

   The port is randomly selected from range of ports above 1023 (above last
   well-known port).

   @return a port number that is unused on success
   @return 0 on failure to find an unused port
*/
int find_unused_random_local_udp_port(void);




/**
   Configure some objects needed to run a test:
    - libcw receiver (+ libcw generator needed by receiver),
    - key source (to monitor serial line)

   @param[in] opts options for test helpers

   @return 0 if setup was successful,
   @return -1 otherwise
*/
int test_helpers_setup(const helpers_opts_t * opts);




/**
   Deconfigure objects configured with test_helpers_setup()
*/
void test_helpers_cleanup(void);




#endif /* #ifndef CWDAEMON_TEST_LIB_MISC_H */

