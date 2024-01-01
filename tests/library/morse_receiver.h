#ifndef CWDAEMON_TEST_LIB_MORSE_RECEIVER_H
#define CWDAEMON_TEST_LIB_MORSE_RECEIVER_H




#include "cw_rec_utils.h"




int morse_receiver_setup(cw_easy_receiver_t * easy_rec, int wpm);
int morse_receiver_desetup(__attribute__((unused)) cw_easy_receiver_t * easy_rec);
void * morse_receiver_thread_fn(void * thread_arg);




#endif /* #ifndef CWDAEMON_TEST_LIB_MORSE_RECEIVER_H */

