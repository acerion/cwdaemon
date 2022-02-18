#ifndef CWDAEMON_TEST_LIB_MISC_H
#define CWDAEMON_TEST_LIB_MISC_H




#include "process.h"
#include "cw_rec_utils.h"




/**
   @brief Ask cwdaemon to play a text message, receive it on serial line console

   @param[in] child cwdaemon child process used for the test
   @param[in] messsage_value text to played

   @return 0 if text was received successfully
   @return -1 otherwise
*/
int cwdaemon_play_text_and_receive(cwdaemon_process_t * child, const char * message_value);




/**
   @brief Find a UDP port that is not used on local machine

   The port is randomly selected from range of ports above 1023 (above last
   well-known port).

   @return a port number that is unused on success
   @return 0 on failure
*/
int find_unused_random_local_udp_port(void);




int test_helpers_setup(int speed);
void test_helpers_teardown(void);




#endif /* #ifndef CWDAEMON_TEST_LIB_MISC_H */

