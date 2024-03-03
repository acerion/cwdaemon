#ifndef CWDAEMON_TEST_LIB_STRING_UTILS_H
#define CWDAEMON_TEST_LIB_STRING_UTILS_H




#include <stdlib.h>




/**
   Replace non-printable characters with their printable representations

   A copy of @buffer with printable characters is placed in @p printable

   @param[in] buffer Input string
   @param[out] printable Preallocated output string
   @param[in] size Size of @p printable buffer

   @return @p printable
*/
char * get_printable_string(const char * buffer, char * printable, size_t size);




#endif /* #ifndef CWDAEMON_TEST_LIB_STRING_UTILS_H */

