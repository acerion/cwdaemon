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

ssize_t cwdaemon_recvfrom(cwdaemon_t * cwdaemon, char * request, size_t size);




#endif /* #ifndef H_CWDAEMON_SOCKET */

