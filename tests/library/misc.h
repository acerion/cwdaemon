#ifndef CWDAEMON_TEST_LIB_MISC_H
#define CWDAEMON_TEST_LIB_MISC_H




#include "cw_rec_utils.h"
#include "cwdevice_observer.h"
#include "process.h"




typedef struct {
	int wpm;
} helpers_opts_t;





/**
   @brief Ask cwdaemon to play a text message, receive it on serial line console

   @param[in] client Client used to send and receive data to/from cwdaemon server
   @param[in] messsage_value text to played
   @param[in] expected_failed_receive whether a receiver is expected to receive the message incorrectly (i.e. not receive it at all)

   @return 0 if text was received successfully
   @return -1 otherwise
*/
int client_send_and_receive(client_t * client, const char * message_value, bool expected_failed_receive);
int client_send_and_receive_2(client_t * client, cw_easy_receiver_t * morse_receiver, const char * message_value, bool expected_failed_receive);




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
    - cwdevice observer (to monitor serial line)

   @param[in] opts options for test helpers
   @param[in] key_source_params parameters for cwdevice observer

   @return 0 if setup was successful,
   @return -1 otherwise
*/
int test_helpers_setup(const helpers_opts_t * opts, const cwdevice_observer_params_t * key_source_params);




/**
   Deconfigure objects configured with test_helpers_setup()
*/
void test_helpers_cleanup(void);




#endif /* #ifndef CWDAEMON_TEST_LIB_MISC_H */

