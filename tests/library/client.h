#ifndef CWDAEMON_TEST_LIB_CLIENT_H
#define CWDAEMON_TEST_LIB_CLIENT_H




/*
  Local client connecting to local or remote cwdaemon server over network socket.
*/




typedef struct client_t {
	int sock;   /**< Network socket used by client to communicate with server. */
} client_t;




/**
   @brief Close client's connection to local or remote cwdaemon server

   @param[in] client Client for which to make a disconnection

   @return 0 on success
   @return -1 on failure
*/
int client_disconnect(client_t * client);




#endif /* #ifndef CWDAEMON_TEST_LIB_CLIENT_H */

