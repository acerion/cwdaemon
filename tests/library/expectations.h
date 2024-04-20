#ifndef CWDAEMON_TESTS_LIB_EXPECTATIONS_H
#define CWDAEMON_TESTS_LIB_EXPECTATIONS_H




#include "client.h"




/**
   @brief Data received in "reply" from cwdaemon must match data sent in
   "reply" Escape request or "caret" request

   @reviewed_on{2024.02.19}

   @param[in] expectation_idx Index/number of expectation - info to be included in logs
   @param[in] received Reply received from tested cwdaemon server
   @param[in] expected Expected reply - what test expects to receive from tested cwdaemon server

   @return 0 if expectation is met
   @return -1 otherwise
*/
int expect_reply_match(int expectation_idx, test_reply_data_t const * received, test_reply_data_t const * expected);




/**
   @brief Text keyed on cwdevice and received by Morse receiver must match text sent to cwdaemon for keying

   Receiving of message by Morse receiver should not be verified if the
   expected message is too short (the problem with "warm-up" of receiver).

   @reviewed_on{2024.02.19}

   @param[in] expectation_idx Index/number of expectation - info to be included in logs
   @param[in] received Morse message keyed by cwdaemon and received on cwdevice by Morse receiver
   @param[in] expected Expected Morse message - what test expects to be keyed by tested cwdaemon server

   @return 0 if expectation is met
   @return -1 otherwise
*/
int expect_morse_match(int expectation_idx, const char * received, const char * expected);




/**
   @brief The end of receiving of Morse message and the time of receiving a
   reply should be separated by short time span

   Evaluate time span between 'reply' event and the end of receiving a Morse
   message.

   Currently (0.12.0) the time span is ~300ms. TODO acerion 2023.12.31:
   shorten the time span in cwdaemon.

   @reviewed_on{2024.02.19}

   @param[in] expectation_idx Index/number of expectation - info to be included in logs
   @param[in] morse_is_earlier Whether Morse event is earlier than reply event
   @param[in] morse_event Event describing receiving of Morse message on cwdevice
   @param[in] reply_event Event describing receiving a reply from tested cwdaemon server

   @return 0 if expectation is met
   @return -1 otherwise
*/
int expect_morse_and_reply_events_distance(int expectation_idx, bool morse_is_earlier, const event_t * morse_event, const event_t * reply_event);




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

   TODO acerion 2024.04.19: This code doesn't cover the case when both events
   are exactly in the same moment (have the same time stamp). How to handle
   such rare case?

   @reviewed_on{2024.02.19}

   @param[in] morse_is_earlier Whether Morse event is earlier than reply event

   @return 0 if expectation is met
   @return -1 otherwise
*/
int expect_morse_and_reply_events_order(int expectation_idx, bool morse_is_earlier);




/**
   @brief Count of recorded events must match count of expected events

   @reviewed_on{2024.02.19}

   @param[in] expectation_idx Index/number of expectation - info to be included in logs
   @param[in] n_recorded Count of recorded events
   @param[in] n_expected Count of expected events

   @return 0 if expectation is met
   @return -1 otherwise
*/
int expect_count_of_events(int expectation_idx, int n_recorded, int n_expected);




#endif /* #ifndef CWDAEMON_TESTS_LIB_EXPECTATIONS_H */

