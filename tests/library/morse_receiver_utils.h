#ifndef CWDAEMON_TEST_LIB_MORSE_RECEIVER_UTILS_H
#define CWDAEMON_TEST_LIB_MORSE_RECEIVER_UTILS_H




#include <stdbool.h>




/**
   @brief Test whether text received through Morse receiver matches expected string

   @warning Because of a known limitation in current implementation of Morse
   receiver, the receiver incorrectly receives first letter. Therefore DON'T
   pass to this function strings that only have one letter. The function will
   always return true for such strings.

   @param[in] received_text Text received by Morse receiver
   @param[in] expected_message The text that we have expected to receive

   @return true if there is a match
   @return false otherwise
*/
bool morse_receive_text_is_correct(const char * received_text, const char * expected_message);




#endif /* #ifndef CWDAEMON_TEST_LIB_MORSE_RECEIVER_UTILS_H */

