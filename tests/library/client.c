/*
 * client.c - Client connecting to local or remote cwdaemon server over
 * network socket.
 *
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
 * Copyright (C) 2012 - 2023 Kamil Ignacak <acerion@wp.pl>
 *
 * Some of this code is taken from netkeyer.c, which is part of the tlf source,
 * here is the copyright:
 * Tlf - contest logging program for amateur radio operators
 * Copyright (C) 2001-2002-2003 Rein Couperus <pa0rct@amsat.org>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client.h"
#include "socket.h"




int client_send_request(client_t * client, int request, const char * value)
{
	char buf[80] = { 0 };

	switch (request) {
	case CWDAEMON_REQUEST_RESET:
		buf[0] = 27;
		sprintf(buf + 1, "0");
		break;
	case CWDAEMON_REQUEST_MESSAGE:
		/* Notice that we don't put Escape character in buf.
		   Regular text message is not an escaped request. */
		sprintf(buf, "%s", value);
		break;
	case CWDAEMON_REQUEST_SPEED:
		buf[0] = 27;
		sprintf(buf + 1, "2");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_TONE:
		buf[0] = 27;
		sprintf(buf + 1, "3");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_ABORT:
		buf[0] = 27;
		sprintf(buf + 1, "4");
		break;
	case CWDAEMON_REQUEST_EXIT:
		buf[0] = 27;
		sprintf(buf + 1, "5");
		break;
	case CWDAEMON_REQUEST_WORDMODE:
		buf[0] = 27;
		sprintf(buf + 1, "6");
		break;
	case CWDAEMON_REQUEST_WEIGHT:
		buf[0] = 27;
		sprintf(buf + 1, "7");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_DEVICE:
		buf[0] = 27;
		sprintf(buf + 1, "8");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_PTT:
		buf[0] = 27;
		sprintf(buf + 1, "a");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_TUNE:
		buf[0] = 27;
		sprintf(buf + 1, "c");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_TOD:
		buf[0] = 27;
		sprintf(buf + 1, "d");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_SDEVICE:
		buf[0] = 27;
		sprintf(buf + 1, "f");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_VOLUME:
		buf[0] = 27;
		sprintf(buf + 1, "g");
		sprintf(buf + 2, "%s", value);
		break;
	case CWDAEMON_REQUEST_REPLY:
		buf[0] = 27;
		sprintf(buf + 1, "h");
		sprintf(buf + 2, "%s", value);
		break;
	default:
		buf[0] = '\0';
		break;
	}

	ssize_t send_rc = -1;
	errno = 0;
	if (buf[0] != '\0') {
		send_rc = send(client->sock, buf, sizeof (buf), 0);
	}

	if (send_rc == -1) {
		fprintf(stderr, "[ERROR] Failed to send request to server: %s.\n", strerror(errno));
		return -1;
	} else {
		return 0;
	}
}




int client_disconnect(client_t * client)
{
	if (NULL == client) {
		return -1;
	}

	if (client->sock < 0) {
		return 0;
	}
	if (close(client->sock) == -1) {
		perror("Failed to close client's socket to cwdaemon server.");
		client->sock = -1;
		return -1;
	}
	client->sock = -1;
	return 0;
}

