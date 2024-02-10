#ifndef CWDAEMON_TEST_LIB_STRING_UTILS_H
#define CWDAEMON_TEST_LIB_STRING_UTILS_H




#include <stdlib.h>




/**
   Replace escaped characters with their un-escaped representations

   A copy of @buffer with expanded characters is placed in @p escaped

   @param[in] buffer Input string
   @param[out] escaped Preallocated output string
   @param[in] size Size of @p escaped buffer

   @return @p escaped
*/
char * escape_string(const char * buffer, char * escaped, size_t size);




#endif /* #ifndef CWDAEMON_TEST_LIB_STRING_UTILS_H */

