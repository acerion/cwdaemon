#ifndef CWDAEMON_TEST_LIB_MISC_H
#define CWDAEMON_TEST_LIB_MISC_H




#include "cw_rec_utils.h"
#include "cwdevice_observer.h"
#include "process.h"




/**
   @brief Find a UDP port that is not used on local machine

   The port is semi-randomly selected from range of non-privileged ports
   (i.e. from range <1024 - 65535>, inclusive. See also
   CWDAEMON_NETWORK_PORT_MIN and CWDAEMON_NETWORK_PORT_MAX).

   The function is slightly biased towards returning cwdaemon's default port
   CWDAEMON_NETWORK_PORT_DEFAULT == 6789.

   Port is returned through @p port function argument.

   @param[out] port Selected port

   @return 0 on success
   @return -1 on failure to find an unused port
*/
int find_unused_random_biased_local_udp_port(in_port_t * port);




/**
   Replace escaped characters with their un-escaped representations

   A copy of @buffer with expanded characters is placed in @p escaped

   @param[in] buffer Input string
   @param[out] escaped Preallocated output string
   @param[in] size Size of @p escaped buffer

   @return @p escaped
*/
char * escape_string(const char * buffer, char * escaped, size_t size);




#endif /* #ifndef CWDAEMON_TEST_LIB_MISC_H */

