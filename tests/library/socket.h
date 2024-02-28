#ifndef CWDAEMON_TEST_LIB_SOCKET_H
#define CWDAEMON_TEST_LIB_SOCKET_H




#include <netinet/in.h>




int open_socket_to_server(const char * server_ip_address, in_port_t server_in_port);




#endif /* #ifndef CWDAEMON_TEST_LIB_SOCKET_H */

