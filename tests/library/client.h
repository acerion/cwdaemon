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
	int sock;                 /**< Network socket used by client to communicate with server. */
	socket_receive_data_t received_data;    /**< Buffer for receiving replies from server. */

	thread_t socket_receiver_thread;

	/** Reference to test's events container. Used to collect events
	    registered during test that are relevant to cwdaemon client. */
	events_t * events;
} client_t;




/**
   @brief Destructor
*/
int client_dtor(client_t * client);




/**
   Send escaped request to cwdaemon server

   Value of request is stored in opaque array @p bytes. There are @p n_bytes
   bytes of data in @b bytes. All @p n_bytes bytes of data are to sent
   through socket.

   If data in @p bytes is representing a C string, it is up to caller of the
   function to have it terminated with NUL and to pass correct value of @p
   n_bytes. The value of @p n_bytes must include terminating NUL of the
   string. Even if caller passes a string to the function, the function
   treats @p bytes as opaque array of some bytes.

   @param[in] request one of CWDAEMON_ESC_REQUEST_* values from src/cwdaemon.h

   @return 0 on successful sending of data
   @return -1 otherwise
*/
int client_send_esc_request(client_t * client, int request, const char * bytes, size_t n_bytes);

/* See comment inside of morse_receive_text_is_correct() to learn why this
   function is needed (in nutshell: cw receiver may mis-receive initial
   letters of message). */
int client_send_esc_request_va(client_t * client, int request, const char * format, ...) __attribute__ ((format (printf, 3, 4)));




/**
   Send text message to cwdaemon server to have it played by cwdaemon

   The message may be a caret message - a text ending with '^' character
   which is interpreted in special way by cwdaemon.

   Value of message is stored in opaque array @p bytes. There are @p n_bytes
   bytes of data in @b bytes. All @p n_bytes bytes of data are to sent
   through socket.

   If data in @p bytes is representing a C string, it is up to caller of the
   function to have it terminated with NUL and to pass correct value of @p
   n_bytes. The value of @p n_bytes must include terminating NUL of the
   string. Even if caller passes a string to the function, the function
   treats @p bytes as opaque array of some bytes.

   @param client cwdaemon client
   @param[in] bytes Array of bytes to be sent over socket
   @param[in] n_bytes Count of bytes in @p bytes to be sent over socket

   @return 0 on successful sending of data
   @return -1 otherwise
*/
int client_send_message(client_t * client, const char * bytes, size_t n_bytes);




int client_connect_to_server(client_t * client, const char * server_ip_address, in_port_t server_in_port);




/**
   @brief Close client's connection to local or remote cwdaemon server

   @param[in] client Client for which to make a disconnection

   @return 0 on success
   @return -1 on failure
*/
int client_disconnect(client_t * client);




int client_socket_receive_enable(client_t * client);
int client_socket_receive_start(client_t * client);
int client_socket_receive_stop(client_t * client);




/**
   @brief Check if bytes received over socket match expected value

   @param[in] expected What is expected
   @param[in] received What has been received over network

   @return true if received data matches expected data
   @return false otherwise
*/
bool socket_receive_bytes_is_correct(const socket_receive_data_t * expected, const socket_receive_data_t * received);




#endif /* #ifndef CWDAEMON_TEST_LIB_CLIENT_H */

