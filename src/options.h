#ifndef CWDAEMON_OPTIONS_H
#define CWDAEMON_OPTIONS_H




#include <netinet/in.h>
#include <stdint.h>




int cwdaemon_option_network_port(in_port_t * port, const char * opt_value);
int cwdaemon_option_libcwflags(uint32_t * flags, const char * opt_value);




#endif /* #ifndef CWDAEMON_OPTIONS_H */

