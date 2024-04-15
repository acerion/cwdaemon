#ifndef TEST_DEFINES_H
#define TEST_DEFINES_H




#include "src/cwdaemon.h"




/**
   This buffer must be large enough to receive replies from correctly behaving
   cwdaemon. The additional integer is for unexpected bytes received from
   cwdaemon.
*/
#define CLIENT_RECV_BUFFER_SIZE (CWDAEMON_REPLY_SIZE_MAX + 64)




/**
   This buffer must be large enough to contain extraordinary amount of data
   that is sent as request to cwdaemon to stress-test it.
*/
#define CLIENT_SEND_BUFFER_SIZE (4 * CWDAEMON_REQUEST_SIZE_MAX)




/**
   Size of buffer for string representation of binary data, where
   non-printable bytes are represented by printable representations.

   The argument to this macro is the size of buffer containing the binary
   data, for which you want to get printable representation.

   Most of non-printable bytes are represented by "{0xXY}" string, i.e. by 6
   bytes.

   '\r' and '\n' are represented by "{CR}" and "{LF}", respectively.
*/
#define PRINTABLE_BUFFER_SIZE(_input_buffer_size_)  (1 + (6 * (_input_buffer_size_)))




/**
   How many letters can a Morse receiver receive?

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
#define TEST_WPM_MIN 10
#define TEST_WPM_MAX 20 /* At 25 we start to have receive errors. */
#define TEST_WPM_DEFAULT 10




/* For long-running tests select a tone that is easy on ears, i.e. not too
   high. */
#define TEST_TONE_EASY 600




/**
   @brief Set "bytes" and "count" of bytes in data structure

   The macro can be used to set contents of test_request_t or
   socket_receive_data_t variables or other "bytes+n_bytes" structure used in
   test code.

   The argument to the macro should be a string literal.

   An implicit terminating NUL of a string literal is NEVER included in the
   bytes to be set or the count of bytes to be set.

   If bytes to be set and the count of bytes to be set should include
   terminating NUL, the string literal passed as argument to the macro should
   contain explicit terminating NUL character, like this:

   TEST_SET_BYTES("Hello, world\0")

   cwdaemon server should be able to handle sent and received data that does
   or doesn't end with NUL. This macro makes it easy to declare such data in
   tests code.
*/
#define TEST_SET_BYTES(_str_) { .n_bytes = ((sizeof (_str_)) - 1), .bytes = {_str_} }




#endif /* #ifndef TEST_DEFINES_H */

