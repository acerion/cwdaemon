#ifndef CWDAEMON_TEST_LIB_EXPECTATIONS_H
#define CWDAEMON_TEST_LIB_EXPECTATIONS_H




#include "client.h"




/**
   @brief Data received in socket reply must match data sent in "esc reply"
   or "caret" request

   @return 0 if expectation is met
   @return -1 otherwise
*/
int expect_socket_reply_match(int expectation_idx, const test_reply_data_t * received, const test_reply_data_t * expected);




/**
   @brief Text received by Morse receiver must match text sent to cwdaemon to
   be keyed by the cwdaemon

   cwdaemon keyed a proper Morse message on cwdevice.

   Receiving of message by Morse receiver should not be verified if the
   expected message is too short (the problem with "warm-up" of receiver).

   @return 0 if expectation is met
   @return -1 otherwise
*/
int expect_morse_receive_match(int expectation_idx, const char * received, const char * expected);




/**
   @brief The end of receiving of Morse message and the time of receiving a
   reply should be separated by close time span

   Evaluate time span between 'reply' event and the end of receiving a Morse
   message.

   Currently (0.12.0) the time span is ~300ms. TODO acerion 2023.12.31:
   shorten the time span in cwdaemon.

   @return 0 if expectation is met
   @return -1 otherwise
*/
int expect_morse_and_reply_events_distance(int expectation_idx, int morse_idx, const event_t * morse_event, int reply_idx, const event_t * reply_event);




/**
   @brief End of Morse receive and the moment of receiving a reply are in proper order (on time scale)

   I'm not 100% sure what the correct order should be in perfect
   implementation of cwdaemon. In 0.12.0 it is "reply" first, and
   then "morse receive" second, unless a message sent to server ends with
   space.

   TODO acerion 2024.01.28: check what SHOULD be the correct order of the two
   events. Some comments in cwdaemon indicate that reply should be sent after
   end of playing Morse.

   TODO acerion 2024.01.26: Double check the corner case with space at the
   end of message.

   @return 0 if expectation is met
   @return -1 otherwise
*/
int expect_morse_and_reply_events_order(int expectation_idx, int morse_idx, int reply_idx);





/**
   @brief Count of recorded events must match count of expected events

   @return 0 if expectation is met
   @return -1 otherwise
*/
int expect_count_of_events(int expectation_idx, int n_recorded, int n_expected);




#endif /* #ifndef CWDAEMON_TEST_LIB_EXPECTATIONS_H */

