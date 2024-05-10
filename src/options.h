#ifndef CWDAEMON_OPTIONS_H
#define CWDAEMON_OPTIONS_H




#include <netinet/in.h>
#include <stdint.h>




/// @brief Parse value of "-p"/"--port" command line option
///
/// @param[out] port Parsed value of network port
/// @param[in] opt_value String with value of command line option
///
/// @return 0 on success
/// @return -1 on failure
int cwdaemon_option_network_port(in_port_t * port, char const * opt_value);




/// @brief Parse value of "-I"/"--libcwflags" command line option
///
/// @param[out] flags Parsed value of flags
/// @param[in] opt_value String with value of command line option
///
/// @return 0 on success
/// @return -1 on failure
int cwdaemon_option_libcwflags(uint32_t * flags, char const * opt_value);




void cwdaemon_option_inc_verbosity(int * threshold);




int cwdaemon_option_set_verbosity(int * threshold, char const * opt_value);




#endif /* #ifndef CWDAEMON_OPTIONS_H */

