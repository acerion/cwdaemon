#ifndef TEST_DEFINES_H
#define TEST_DEFINES_H




#include "src/cwdaemon.h"




/**
   This buffer must be large enough to receive replies from correctly behaving
   cwdaemon. The additional integer is for unexpected bytes received from
   cwdaemon.
*/
#define CLIENT_RECV_BUFFER_SIZE (CWDAEMON_MESSAGE_SIZE_MAX + 64)




/**
   This buffer must be large enough to contain extraordinary amount of data
   that is sent to stress-test cwdaemon.
*/
#define CLIENT_SEND_BUFFER_SIZE (4 * CWDAEMON_MESSAGE_SIZE_MAX)




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




/* Lower and upper limit of Morse code speeds used in functional tests. */
#define TEST_WPM_MIN 10
#define TEST_WPM_MAX 15
#define TEST_WPM_DEFAULT 10




#endif /* #ifndef TEST_DEFINES_H */

