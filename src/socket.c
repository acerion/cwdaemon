/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2023 Kamil Ignacak <acerion@wp.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




#include <arpa/inet.h> /* htons() */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>




#include "log.h"
#include "socket.h"




/**
   \brief Initialize network variables in @p cwdaemon

   Initialize network socket and other network variables.

   \param cwdaemon cwdaemon instance

   \return false on failure
   \return true on success
*/
bool cwdaemon_initialize_socket(cwdaemon_t * cwdaemon)
{
	memset(&cwdaemon->request_addr, '\0', sizeof (cwdaemon->request_addr));
	cwdaemon->request_addr.sin_family = AF_INET;
	cwdaemon->request_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	cwdaemon->request_addr.sin_port = htons(cwdaemon->network_port);
	cwdaemon->request_addrlen = sizeof (cwdaemon->request_addr);

	cwdaemon->socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
	if (cwdaemon->socket_descriptor == -1) {
		cwdaemon_errmsg("Socket open");
		return false;
	}

	if (bind(cwdaemon->socket_descriptor,
		 (struct sockaddr *) &cwdaemon->request_addr,
		 cwdaemon->request_addrlen) == -1) {

		cwdaemon_errmsg("Bind");
		return false;
	}

	int save_flags = fcntl(cwdaemon->socket_descriptor, F_GETFL);
	if (save_flags == -1) {
		cwdaemon_errmsg("Trying get flags");
		return false;
	}
	save_flags |= O_NONBLOCK;

	if (fcntl(cwdaemon->socket_descriptor, F_SETFL, save_flags) == -1) {
		cwdaemon_errmsg("Trying non-blocking");
		return false;
	}

	return true;
}




void cwdaemon_close_socket(cwdaemon_t * cwdaemon)
{
	if (-1 != cwdaemon->socket_descriptor) {
		if (close(cwdaemon->socket_descriptor) == -1) {
			cwdaemon_errmsg("Close socket");
			exit(EXIT_FAILURE);
		}
		cwdaemon->socket_descriptor = -1;
	}

	return;
}




/**
   \brief Wrapper around sendto()

   Wrapper around sendto(), sending a \p reply to client.
   Client is specified by reply_* network variables.

   \param cwdaemon cwdaemon instance
   \param reply - reply to be sent

   \return -1 on failure
   \return number of characters sent on success
*/
ssize_t cwdaemon_sendto(cwdaemon_t * cwdaemon, const char *reply)
{
	size_t len = strlen(reply);
	/* TODO: Do we *really* need to end replies with CRLF? */
	assert(reply[len - 2] == '\r' && reply[len - 1] == '\n');

	ssize_t rv = sendto(cwdaemon->socket_descriptor, reply, len, 0,
			    (struct sockaddr *) &cwdaemon->reply_addr, cwdaemon->reply_addrlen);

	if (rv == -1) {
		cwdaemon_debug(CWDAEMON_VERBOSITY_E, __func__, __LINE__, "sendto: \"%s\"", strerror(errno));
		return -1;
	} else {
		return rv;
	}
}




/**
   \brief Receive request sent through socket

   Received request is returned through \p request.
   Possible trailing '\r' and '\n' characters are stripped.
   Request is ended with '\0'.

   \param cwdaemon cwdaemon instance
   \param request buffer for received request
   \param[in] size size of the buffer

   \return -2 if peer has performed an orderly shutdown
   \return -1 if an error occurred during call to recvfrom
   \return  0 if no request has been received
   \return length of received request otherwise
 */
ssize_t cwdaemon_recvfrom(cwdaemon_t * cwdaemon, char *request, size_t size)
{
	ssize_t recv_rc = recvfrom(cwdaemon->socket_descriptor,
				   request,
				   size,
				   0, /* flags */
				   (struct sockaddr *) &cwdaemon->request_addr,
				   /* TODO: request_addrlen may be modified. Check it. */
				   &cwdaemon->request_addrlen);

	if (recv_rc == -1) { /* No requests available? */

		if (errno == EAGAIN || errno == EWOULDBLOCK) { /* "a portable application should check for both possibilities" */
			/* Yup, no requests available from non-blocking socket. Good luck next time. */
			/* TODO: how much CPU time does it cost to loop waiting for a request?
			   Would it be reasonable to configure the socket as blocking?
			   How large is receive timeout? */

			return 0;
		} else {
			/* Some other error. May be a serious error. */
			cwdaemon_errmsg("Recvfrom");
			return -1;
		}
	} else if (recv_rc == 0) {
		/* "peer has performed an orderly shutdown" */
		return -2;
	} else {
		; /* pass */
	}

#if 1 /* Just for debug. */
	/* Potential lack of terminating NUL is not an error of client, this is
	   just a fact that we have to deal with. cwdaemon should be able to
	   safely handle array of arbitrary bytes that is not terminated with
	   NUL. */
	const bool terminating_nul = request[recv_rc - 1] == '\0';
	log_debug("received %zd / %zu bytes, terminating NUL %s found",
	          recv_rc, size, terminating_nul ? "is" : "is not");
#endif

	/* Remove CRLF if present. TCP buffer may end with '\n', so make
	   sure that every request is consistently ended with NUL only.
	   Do it early, do it now. */
	int z = 0;
	while (recv_rc > 0
	       && ( (z = request[recv_rc - 1]) == '\n' || z == '\r') ) {

		request[--recv_rc] = '\0';
	}

	return recv_rc;
}





