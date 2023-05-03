#ifndef CWDAEMON_UTILS
#define CWDAEMON_UTILS




#include <stddef.h> /* size_t */




/**
	@brief Get full path to device file in /dev/ from given input

	Function naively builds a full path to Linux device from given @p input by
	checking for "/dev/" prefix in @p input and pre-pending it if necessary.

	The @p input can be just a name of device in /dev/ directory (e.g.
	"ttyS0"). In that case the function succeeds and returns a new string, a
	full path to the device.

	The @p input can already be a full path to device in /dev/ directory (e.g.
	"/dev/ttyUSB0), then the function also succeeds and a copy of @p input is
	returned.

	On success function returns result through @p path argument (unless
	resulting string is too long).

	The function does not canonicalize the result.

	The function does not test for existence of file indicated by resulting
	path.

	@param[out] path Preallocated buffer for result (for path to device)
	@param[in] size Size of buffer (including space for terminating NUL)
	@param[in] input Input string to use to build the path

	@return 0 on success
	@return (-EINVAL) if any argument is invalid (e.g. NULL pointer)
	@return (-ENAMETOOLONG) if @p path is too small to fit resulting path
*/
int build_full_device_path(char * path, size_t size, const char * input);




#endif /* #ifndef CWDAEMON_UTILS */

