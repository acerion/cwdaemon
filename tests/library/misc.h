#ifndef CWDAEMON_TEST_LIB_MISC_H
#define CWDAEMON_TEST_LIB_MISC_H




#include "cw_rec_utils.h"
#include "cwdevice_observer.h"
#include "process.h"




typedef struct {
	int wpm;
} helpers_opts_t;





/**
   @brief Find a UDP port that is not used on local machine

   The port is randomly selected from range of ports above 1023 (above last
   well-known port).

   @return a port number that is unused on success
   @return 0 on failure to find an unused port
*/
int find_unused_random_local_udp_port(void);




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

