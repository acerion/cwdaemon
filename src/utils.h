#ifndef CWDAEMON_UTILS_H
#define CWDAEMON_UTILS_H




#include <stdbool.h>
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




/*! Status of searching for value in option string. */
typedef enum {
	opt_success,       /*!< Success: found a keyword followed by '=' char followed by value sub-string (the value sub-string may be empty). */
	opt_key_not_found, /*!< Failure: keyword not found in input string. */
	opt_eq_not_found,  /*!< Failure: no '=' character in input string. */
	opt_extra_spaces   /*!< Failure: unexpected spaces in input string. */
} opt_t;

/**
   @brief Look for a keyword in "key=value" option string, return pointer to value sub-string.

   This is not an optimal function, but allows me to re-use code and write
   unit tests for searching values of keys.

   This function doesn't allow spaces around '=' character.
   This function allows for empty value sub-string.

   Searching for keyword is case-insensitive.

   Pointer to value sub-string is returned through @p value arg. The pointer
   points inside of @p input.

   @param[in] input Input string to parse
   @param[in] keyword Keyword to look for in the input
   @param[out] value Pointer to beginning of value sub-string in @p input (the sub-string may turn out to be empty)

   @return value of opt_t indicating status of searching
*/
opt_t find_opt_value(const char * input, const char * keyword, const char ** value);




/**
   \brief Parse a string with 'long' integer

   Parse a string with digits, convert it to a long integer

   \param[in] buf input buffer with a string
   \param[out] lvp pointer to output long int variable

   \return false on failure
   \return true on success
*/
bool cwdaemon_get_long(const char *buf, long *lvp);




#endif /* #ifndef CWDAEMON_UTILS_H */

