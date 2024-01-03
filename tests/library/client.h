#ifndef CWDAEMON_TEST_LIB_CLIENT_H
#define CWDAEMON_TEST_LIB_CLIENT_H




/*
  Local client connecting to local or remote cwdaemon server over network socket.
*/




typedef struct client_t {
	int sock;                 /**< Network socket used by client to communicate with server. */
	char reply_buffer[64];    /**< Buffer for receiving replies from server. */
} client_t;




int client_send_request(client_t * client, int request, const char * value);
int client_send_request_va(client_t * client, int request, const char * format, ...) __attribute__ ((format (printf, 3, 4)));




/**
   @brief Close client's connection to local or remote cwdaemon server

   @param[in] client Client for which to make a disconnection

   @return 0 on success
   @return -1 on failure
*/
int client_disconnect(client_t * client);




void * client_socket_receiver_thread_fn(void * thread_arg);




#endif /* #ifndef CWDAEMON_TEST_LIB_CLIENT_H */

