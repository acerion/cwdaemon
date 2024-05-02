#ifndef TEST_DEFINES_H
#define TEST_DEFINES_H




#include "src/cwdaemon.h"




/// @file
///
/// Miscellaneous useful definition used in tests.




/**
   Client's "receive" buffer must be large enough to receive replies from
   correctly behaving cwdaemon. The additional integer is for unexpected
   bytes received from cwdaemon.
*/
#define CLIENT_RECV_BUFFER_SIZE (CWDAEMON_REPLY_SIZE_MAX + 64)




/**
   Client's "send" buffer must be large enough to contain extraordinary
   amount of data that is sent as request to cwdaemon to stress-test it.
*/
#define CLIENT_SEND_BUFFER_SIZE (4 * CWDAEMON_REQUEST_SIZE_MAX)




/**
   Size of buffer for string representation of binary data, where
   non-printable bytes are represented by printable representations.

   The argument to this macro is the size of buffer containing the binary
   data, for which you want to get printable representation.

   The longest representation of a non-printable byte looks like this:
   "{0xXY}", i.e. it consists of 6 bytes.

   +1 for terminating NUL.
*/
#define PRINTABLE_BUFFER_SIZE(_input_buffer_size_)  ((6 * (_input_buffer_size_)) + 1)




/**
   How many characters (including inter-word-spaces) can a Morse receiver
   receive?

   If a client asks cwdaemon to play N characters, and a Morse receiver is
   very bad at receiving them and misrepresents the characters, the receiver
   must be able to store more characters than the cwdaemon plays. Therefore
   the size of the buffer is "4 * N", +1 for terminating NUL.
*/
#define MORSE_RECV_BUFFER_SIZE ((4 * CLIENT_SEND_BUFFER_SIZE) + 1)




/** Size of buffer for string representation (description) of errno. */
#define ERRNO_BUF_SIZE 64




/**
   Size of buffer for string representation of network port number.

   Must be big enough to store invalid values
*/
#define PORT_BUF_SIZE (sizeof ("some totally invalid value of network port"))




/**
   Size of buffer for string representation of Morse code speed.

   Must be big enough to store invalid values
*/
#define WPM_BUF_SIZE (sizeof ("some totally invalid value of Morse code speed"))




/**
   Size of buffer for string representation of tone (frequency).

   Must be big enough to store invalid values
*/
#define TONE_BUF_SIZE (sizeof ("some totally invalid value of tone (frequency)"))




/* Lower and upper limit of Morse code speeds used in functional tests. */
#define TESTS_WPM_MIN 10
#define TESTS_WPM_MAX 20 /* At 25 we start to have receive errors. TODO (acerion) 2024.04.19: fix the receiving and increase the limit. */
#define TESTS_WPM_DEFAULT 10 /* TODO (acerion) 2024.04.19: increase the default at some point in the future. */




/* For long-running tests select a tone that is easy on ears, i.e. not too
   high. But not too low either - too low tone may be difficult to hear. */
#define TESTS_TONE_EASY 600




/**
   @brief Set "bytes" and "count" of bytes in data structure

   The macro can be used to set contents of test_request_t or
   test_reply_data_t variables or other "bytes+n_bytes" structure used in
   test code.

   The argument to the macro should be a string literal.

   An implicit terminating NUL of a string literal is NEVER included in the
   bytes to be set or the count of bytes to be set.

   If bytes to be set and the count of bytes to be set should include
   terminating NUL, the string literal passed as argument to the macro should
   contain explicit terminating NUL character, like this:

   TESTS_SET_BYTES("Hello, world\0")

   cwdaemon server should be able to handle sent and received data that does
   or doesn't end with NUL. This macro makes it easy to declare such data in
   tests code.
*/
#define TESTS_SET_BYTES(_str_) { .n_bytes = ((sizeof (_str_)) - 1), .bytes = {_str_} }






/// @brief Set fields of event_morse_receive_t variable
///
/// The macro can be used to set contents of event_morse_receive_t.
///
///The argument to the macro should be a string literal.
#define TESTS_SET_MORSE(_str_) { .string = {_str_} }




#endif /* #ifndef TEST_DEFINES_H */

