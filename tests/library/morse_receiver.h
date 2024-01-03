#ifndef CWDAEMON_TEST_LIB_MORSE_RECEIVER_H
#define CWDAEMON_TEST_LIB_MORSE_RECEIVER_H




#include "cw_rec_utils.h"
#include "misc.h"




typedef struct morse_receiver_config_t {
	tty_pins_t observer_tty_pins_config;
	int wpm;                                 /**< Receiver speed (words-per-minute), for receiver working in non-adaptive mode (which is usually the case). */
} morse_receiver_config_t;




int morse_receiver_setup(cw_easy_receiver_t * easy_rec, int wpm);
int morse_receiver_desetup(__attribute__((unused)) cw_easy_receiver_t * easy_rec);
void * morse_receiver_thread_fn(void * thread_arg);




/**
   @brief Test whether text received through Morse receiver matches expected string

   @param[in] received_text Text received by Morse receiver
   @param[in] expected_message The text that we have expected to receive

   @return true if there is a match
   @return false otherwise
*/
bool morse_receive_text_is_correct(const char * received_text, const char * expected_message);




#endif /* #ifndef CWDAEMON_TEST_LIB_MORSE_RECEIVER_H */

