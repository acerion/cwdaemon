#ifndef CWDAEMON_TEST_LIB_MISC_H
#define CWDAEMON_TEST_LIB_MISC_H




#include "process.h"
#include "cw_rec_utils.h"




typedef struct {
	int id;
	char value[32];
	char requested_reply[32];
} cwdaemon_request_t;




int cwdaemon_request_message_and_receive(cwdaemon_process_t * child, cwdaemon_request_t * request, cw_easy_receiver_t * easy_rec);

/**
   @brief Find a UDP port that is not used on local machine

   The port is randomly selected from range of ports above 1023 (above last
   well-known port).

   @return a port number that is unused on success
   @return 0 on failure
*/
int find_unused_random_local_udp_port(void);




#endif /* #ifndef CWDAEMON_TEST_LIB_MISC_H */

