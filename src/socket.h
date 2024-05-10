#ifndef H_CWDAEMON_SOCKET
#define H_CWDAEMON_SOCKET




#include <stdbool.h>
#include <stdint.h>




#include "cwdaemon.h"




bool    cwdaemon_initialize_socket(cwdaemon_t * cwdaemon);
void    cwdaemon_close_socket(cwdaemon_t * cwdaemon);




/**
   @brief Wrapper around sendto()

   Wrapper around sendto(), sending a @p reply to client. Client is specified
   by reply_* members of @p cwdaemon argument.

   @param cwdaemon cwdaemon instance
   @param[in] reply array of bytes to be sent over socket

   @return -1 on failure
   @return number of characters sent on success
*/
ssize_t cwdaemon_sendto(cwdaemon_t * cwdaemon, const char * reply);




//// @brief Receive request through socket
///
/// Received request is returned through \p request.
///
/// Possible trailing '\r' and '\n' characters are replaced with '\0'. Other
/// than that, the function doesn't add terminating NUL to @p request. When
/// the replacement happens, the function returns length of trimmed request,
/// and the length doesn't include the inserted NUL character(s).
///
/// @param cwdaemon cwdaemon instance
/// @param[out] request buffer for received request
/// #param[in] size size of the buffer
///
/// @return -2 if peer has performed an orderly shutdown
/// @return -1 if an error occurred during call to recvfrom
/// @return  0 if no request has been received
/// @return length of received request otherwise
ssize_t cwdaemon_recvfrom(cwdaemon_t * cwdaemon, char * request, size_t size);




#endif /* #ifndef H_CWDAEMON_SOCKET */

