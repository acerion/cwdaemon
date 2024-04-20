#ifndef CWDAEMON_TESTS_LIB_SOCKET_H
#define CWDAEMON_TESTS_LIB_SOCKET_H




#include <netinet/in.h>




/// @brief Open an UDP socket to local or remote cwdaemon server
///
/// TODO (acerion) 2024.04.14: the first arg SHOULD be a "host", allowing for
/// specification of cwdaemon server either by IP address or domain name.
/// Make sure to test a case where host running a cwdaemon server is
/// specified by domain name
//
/// @reviewed_on{2024.04.19}
///
/// @param[in] server_ip_address IP address of host running cwdaemon server
/// @param[in] server_in_port network port on which the cwdaemon server is listening
///
/// @return opened network socket on success
/// @return -1 on failure
int open_socket_to_server(const char * server_ip_address, in_port_t server_in_port);




#endif /* #ifndef CWDAEMON_TESTS_LIB_SOCKET_H */

