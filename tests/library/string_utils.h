#ifndef CWDAEMON_TESTS_LIB_STRING_UTILS_H
#define CWDAEMON_TESTS_LIB_STRING_UTILS_H




#include <stdlib.h>




/**
   @brief Get a copy of string in which non-printable characters are represented with their printable form

   A representation of @bytes with printable characters is placed in @p
   printable.

   Use PRINTABLE_BUFFER_SIZE() macro in code calling this function to define
   size of @p printable buffer.

   @param[in] bytes Input array of bytes to convert
   @param[in] n_bytes Count of bytes in @p bytes
   @param[out] printable Preallocated output string
   @param[in] size Size of @p printable buffer

   @return @p printable
*/
char * get_printable_string(const char * bytes, size_t n_bytes, char * printable, size_t size);




#endif /* #ifndef CWDAEMON_TESTS_LIB_STRING_UTILS_H */

