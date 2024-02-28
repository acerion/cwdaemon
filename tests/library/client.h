#ifndef CWDAEMON_TEST_LIB_CLIENT_H
#define CWDAEMON_TEST_LIB_CLIENT_H




/*
  Local client connecting to local or remote cwdaemon server over network socket.
*/




#include <netinet/in.h>

#include <src/cwdaemon.h>

#include "events.h"
#include "thread.h"




/*
  Just text to play, without "<ESC>X" header.

  This is an addition to CWDAEMON_ESC_REQUEST_* values defined in
  src/cwdaemon.h.
*/
#define CWDAEMON_REQUEST_MESSAGE  0




typedef struct client_t {
	int sock;                 /**< Network socket used by client to communicate with server. */
	char reply_buffer[64];    /**< Buffer for receiving replies from server. */

	thread_t socket_receiver_thread;

	/** Reference to test's events container. Used to collect events
	    registered during test that are relevant to cwdaemon client. */
	events_t * events;
} client_t;




/**
   @brief Destructor
*/
int client_dtor(client_t * client);




int client_send_request(client_t * client, int request, const char * value);

/* See comment inside of morse_receive_text_is_correct() to learn why this
   function is needed (in nutshell: cw receiver may mis-receive initial
   letters of message). */
int client_send_request_va(client_t * client, int request, const char * format, ...) __attribute__ ((format (printf, 3, 4)));





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





#endif /* #ifndef CWDAEMON_TEST_LIB_CLIENT_H */

