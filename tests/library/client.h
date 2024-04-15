#ifndef CWDAEMON_TEST_LIB_CLIENT_H
#define CWDAEMON_TEST_LIB_CLIENT_H




/*
  Local client connecting to local or remote cwdaemon server over network socket.
*/




#include <netinet/in.h>

#include <src/cwdaemon.h>

#include "events.h"
#include "test_defines.h"
#include "thread.h"




typedef struct client_t {
	int sock;                              ///< Network socket used by client to communicate with server. Set to (-1) when unused/closed.
	socket_receive_data_t received_data;   ///< Buffer for receiving replies from server.

	thread_t socket_receiver_thread;       ///< Thread receiving data over socket from cwdaemon server.

	/** Reference to test's events container. Used to collect events
	    registered during test that are relevant to cwdaemon client. */
	events_t * events;
} client_t;




/// @brief Destructor of cwdaemon client
///
/// Destruct the variable. Client code must call client_disconnect() before
/// calling the destructor.
///
/// @reviewed_on{2024.04.14}
///
/// @return 0 on success
/// @return -1 on failure
int client_dtor(client_t * client);




/**
   @ Send Escape request to cwdaemon server

   Value of request is stored in opaque array @p bytes. There are @p n_bytes
   bytes of data in @b bytes. All @p n_bytes bytes of data are sent through
   client's socket.

   If data in @p bytes is representing a C string, it is up to caller of the
   function to have it terminated with NUL and to pass correct value of @p
   n_bytes. The value of @p n_bytes must include terminating NUL of the
   string. Even if caller passes a string to the function, the function
   treats @p bytes as opaque array of some bytes.

   @reviewed_on{2024.04.14}

   @param client cwdaemon client used to communicate with cwdaemon server
   @param[in] code one of CWDAEMON_ESC_REQUEST_* values from src/cwdaemon.h
   @param[in] bytes data to be sent to cwdaemon server
   @param[in] n_bytes count of bytes in @p bytes

   @return 0 on successful sending of data
   @return -1 otherwise
*/
int client_send_esc_request(client_t * client, int code, const char * bytes, size_t n_bytes);




/**
   @brief Send an opaque request to cwdaemon server

   Value and size of data is stored in opaque request @p request. All @p
   request->n_bytes bytes of data are sent through client's socket.

   @reviewed_on{2024.04.14}

   @param client cwdaemon client
   @param[in] request Request to be sent to cwdaemon server

   @return 0 on successful sending of data
   @return -1 otherwise
*/
int client_send_request(client_t * client, const test_request_t * request);




/// @brief Connect given cwdaemon client to cwdaemon server that listens on given host/port
///
/// TODO (acerion) 2024.04.14: the second arg SHOULD be a "host", allowing
/// for specification of cwdaemon server either by IP address or domain name.
/// Make sure to test a case where host running a cwdaemon server is
/// specified by domain name.
///
/// Use client_disconnect() to disconnect cwdaemon client from cwdaemon
/// server.
///
/// @reviewed_on{2024.04.14}
///
/// @param client cwdaemon client that should connect to a server
/// @param[in] server_ip_address IP address of host running cwdaemon server
/// @param[in] server_in_port network port on which the cwdaemon server is listening
///
/// @return 0 on success
/// @return -1 on failure
int client_connect_to_server(client_t * client, const char * server_ip_address, in_port_t server_in_port);




/**
   @brief Close client's connection to local or remote cwdaemon server

   This function disconnects a connection made with
   client_connect_to_server().

   @reviewed_on{2024.04.14}

   @param[in] client Client to be disconnected

   @return 0 on success
   @return -1 on failure
*/
int client_disconnect(client_t * client);




/// @brief Enable receiving socket replies from cwdaemon server
///
/// The functionality of receiving replies from cwdaemon server is disabled
/// by default: cwdaemon client can only send requests because this is the
/// most common use case. To have it also receive replies, call this
/// function.
///
/// This function only enables and configures the receiving. The receiving
/// must be also started with client_socket_receive_start().
///
/// @reviewed_on{2024.04.14}
///
/// @param client cwdaemon client on which to enable receiving of replies
///
/// @return 0
int client_socket_receive_enable(client_t * client);




/// @brief Start the thread that waits for replies sent by cwdaemon server
///
/// @param client cwdaemon client that will start waiting/listening for server's replies
///
/// @reviewed_on{2024.04.14}
///
/// @return 0 on success
/// @return -1 on failure
int client_socket_receive_start(client_t * client);



/// @brief Stop the thread that waits for replies sent by cwdaemon server
///
/// @param client cwdaemon client that should stop waiting/listening for server's replies
///
/// @reviewed_on{2024.04.14}
///
/// @return 0 on success
/// @return -1 on failure
int client_socket_receive_stop(client_t * client);




/**
   @brief Check if bytes received over socket from cwdaemon server match expected value

   @reviewed_on{2024.04.14}

   @param[in] expected The data that we expect to receive from cwdaemon server
   @param[in] received The data that we actually received from cwdaemon server

   @return true if received data matches expected data
   @return false otherwise
*/
bool socket_receive_bytes_is_correct(const socket_receive_data_t * expected, const socket_receive_data_t * received);




#endif /* #ifndef CWDAEMON_TEST_LIB_CLIENT_H */

