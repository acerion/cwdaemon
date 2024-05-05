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
   @brief The end of receiving of Morse message and the time of receiving a
   reply should be separated by short time span

   Find Morse and reply events in array of recorded events, Evaluate time
   span between 'reply' event and the end of receiving a Morse message.

   The function should be called only after
   expect_count_type_order_contents() returned success.

   Currently (0.12.0) the time span is ~300ms. TODO acerion 2023.12.31:
   shorten the time span in cwdaemon.

   @reviewed_on{2024.05.01}

   @param[in] expectation_idx Index/number of expectation - info to be included in logs
   @param[in] recorded_events Array of recorded events

   @return 0 if expectation is met
   @return -1 otherwise
*/
int expect_morse_and_reply_events_distance(int expectation_idx, event_t const * recorded_events);




/// @brief EXIT request sent to server, and SIGHLD received in test program should be separated by short time span
///
/// Find "EXIT request" and reply events in array of recorded events,
/// Evaluate time span between the two events.
///
/// The function should be called only after
/// expect_count_type_order_contents() returned success.
///
/// @reviewed_on{2024.05.03}
///
/// @param[in] expectation_idx Index/number of expectation - info to be included in logs
/// @param[in] recorded_events Array of recorded events
///
/// @return 0 if expectation is met
/// @return -1 otherwise
int expect_exit_and_sigchld_events_distance(int expectation_idx, event_t const * recorded_events);




/// @brief Compare arrays of expected and recorded events
///
/// The function compares contents of events from both arrays, with exception
/// of time stamps.
///
/// The function can detect if recorded events aren't in the same order as
/// expected events. The function can also detect different count of events
/// in the two arrays.
///
/// @reviewed_on{2024.05.01}
///
/// @param[in] expectation_idx Index/number of expectation - info to be included in logs
/// @param[in] expected Array of expected events
/// @param[in] recorded Array of recorded events
///
/// @return 0 if both arrays contain the same events
/// @return -1 otherwise
int expect_count_type_order_contents(int expectation_idx, event_t const * expected, event_t const * recorded);




#endif /* #ifndef CWDAEMON_TESTS_LIB_EXPECTATIONS_H */

