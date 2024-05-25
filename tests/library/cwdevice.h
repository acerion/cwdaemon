#ifndef TESTS_LIBRARY_CWDEVICE_H
#define TESTS_LIBRARY_CWDEVICE_H




#include <stdlib.h>




/// @brief Get absolute path to cwdevice with given @p name
///
/// Function returns an absolute path to a device in /dev dir. The function
/// is not smart, it doesn't canonicalize the path. Result is put into @p
/// path that should be allocated by caller.
///
/// A "null" device name is copied to output as-is.
///
/// Examples:
///   "ttyUSB"       ->   "/dev/ttyUSB0"
///   "/dev/ttyS0"   ->   "/dev/ttyS0"
///   "null"         ->   "null"
///
/// @param[in] name Name of cwdevice (e.g. "ttyUSB0" or "/dev/ttyUSB0")
/// @param[out] path Buffer into which a path will be put
/// @param[in] size Size of @p path
///
/// @return @path
char * cwdevice_get_full_path(char const * name, char * path, size_t size);




#endif // #ifndef TESTS_LIBRARY_CWDEVICE_H

