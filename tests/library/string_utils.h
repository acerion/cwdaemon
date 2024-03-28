#ifndef CWDAEMON_TEST_LIB_STRING_UTILS_H
#define CWDAEMON_TEST_LIB_STRING_UTILS_H




#include <stdlib.h>




/**
   Replace non-printable characters with their printable representations

   A representation of @bytes with printable characters is placed in @p
   printable.

   @param[in] bytes Input array of bytes to convert
   @param[in] n_bytes Count of bytes in @p bytes
   @param[out] printable Preallocated output string
   @param[in] size Size of @p printable buffer

   @return @p printable
*/
char * get_printable_string(const char * bytes, size_t n_bytes, char * printable, size_t size);




#endif /* #ifndef CWDAEMON_TEST_LIB_STRING_UTILS_H */

