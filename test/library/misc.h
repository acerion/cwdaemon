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




#endif /* #ifndef CWDAEMON_TEST_LIB_MISC_H */

