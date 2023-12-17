/*
 * client.c - Client connecting to local or remote cwdaemon server over
 * network socket.
 *
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */


#include <stdio.h>
#include <unistd.h>

#include "client.h"




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

