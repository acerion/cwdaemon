/*
 * example.c - example program for cwdaemon
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
 * Copyright (C) 2012 - 2022 Kamil Ignacak <acerion@wp.pl>
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

/*
 * Compile this program with "gcc -o example example.c"
 * Usage: 'example' or 'example <portname>'
 */

#define _POSIX_C_SOURCE 200112L /* getaddrinfo() and friends. */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>




#define K_RESET        0
#define K_MESSAGE      1
#define K_SPEED        2
#define K_TONE         3
#define K_ABORT        4
#define K_STOP         5     // Tell cwdaemon process to exit cleanly. Also known as EXIT.
#define K_WORDMODE     6
#define K_WEIGHT       7
#define K_DEVICE       8
#define K_TOD          9     // set txdelay (turn on delay)
#define K_ADDRESS     10     // set port address of device (obsolete)
#define K_SET14       11     // set pin 14 on lpt
#define K_TUNE        12     // tune
#define K_PTT         13     // PTT on/off
#define K_SWITCH      14     // set band switch output pins 2,7,8,9 on lpt
#define K_SDEVICE     15     // set sound device
#define K_VOLUME      16     // volume for soundcard
#define K_REPLY       17     // Ask cwdaemon to send specified reply after playing text.




static char g_netkeyer_hostaddress[] = "127.0.0.1";
static char g_netkeyer_port[] = "6789";
static int g_socket = -1;

static int netkeyer_init(const char * address, const char * port);
static int netkeyer_close(int fd);
static int netkeyer(int fd, int request, const char * value);
static void catchint(int signal);




static int netkeyer_init(const char * address, const char * port)
{
	/* Code in this function has been copied from
	   getaddrinfo() man page. */

	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = IPPROTO_UDP;

	struct addrinfo * result = NULL;
	int rv = getaddrinfo(address, port, &hints, &result);
	if (rv) {
		fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(rv));
		return -1;
	}

	/* getaddrinfo() returns a list of address structures. Try each
	   address until we successfully connect(2). If socket(2) (or
	   connect(2)) fails, we close the socket and try the next
	   address. */
	int fd = -1;
	for (struct addrinfo * rp = result; rp; rp = rp->ai_next) {
		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd == -1) {
			continue;
		}

		if (connect(fd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}

		close(fd);
		fd = -1;
	}
	freeaddrinfo(result); /* No longer needed */

	if (-1 == fd) {
		fprintf(stderr, "Could not connect\n");
	}
	return fd;
}




static int netkeyer_close(int fd)
{
	if (fd < 0) {
		return 0;
	}
	if (close(fd) == -1) {
		perror("close call failed");
		return -1;
	}
	return 0;
}




static int netkeyer(int fd, int request, const char * value)
{
	char buf[80] = { 0 };

	switch (request) {
		case K_RESET:
			buf[0] = 27;
			sprintf(buf + 1, "0");
			break;
		case K_MESSAGE:
			/* Notice that we don't put Escape character in buf.
			   Regular text message is not an escaped request. */
			sprintf(buf, "%s", value);
			break;
		case K_SPEED:
			buf[0] = 27;
			sprintf(buf + 1, "2");
			sprintf(buf + 2, "%s", value);
			break;
		case K_TONE:
			buf[0] = 27;
			sprintf(buf + 1, "3");
			sprintf(buf + 2, "%s", value);
			break;
		case K_ABORT:
			buf[0] = 27;
			sprintf(buf + 1, "4");
			break;
		case K_STOP:
			buf[0] = 27;
			sprintf(buf + 1, "5");
			break;
		case K_WORDMODE:
			buf[0] = 27;
			sprintf(buf + 1, "6");
			break;
		case K_WEIGHT:
			buf[0] = 27;
			sprintf(buf + 1, "7");
			sprintf(buf + 2, "%s", value);
			break;
		case K_DEVICE:
			buf[0] = 27;
			sprintf(buf + 1, "8");
			sprintf(buf + 2, "%s", value);
			break;
		case K_PTT:
			buf[0] = 27;
			sprintf(buf + 1, "a");
			sprintf(buf + 2, "%s", value);
			break;
		case K_TUNE:
			buf[0] = 27;
			sprintf(buf + 1, "c");
			sprintf(buf + 2, "%s", value);
			break;
		case K_TOD:
			buf[0] = 27;
			sprintf(buf + 1, "d");
			sprintf(buf + 2, "%s", value);
			break;
		case K_SDEVICE:
			buf[0] = 27;
			sprintf(buf + 1, "f");
			sprintf(buf + 2, "%s", value);
			break;
		case K_VOLUME:
			buf[0] = 27;
			sprintf(buf + 1, "g");
			sprintf(buf + 2, "%s", value);
			break;
		case K_REPLY: /* TODO 2022.01.26: test this. */
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
		send_rc = send(fd, buf, sizeof (buf), 0);
	}

	if (send_rc == -1) {
		printf("Keyer send failed (%s)!\n", strerror(errno));
		return -1;
	} else {
		return 0;
	}
}




static void catchint(__attribute__((unused)) int signal)
{
	__attribute__((unused)) int result = netkeyer(g_socket, K_ABORT, "");
	exit(EXIT_SUCCESS);
}




int main(int argc, char **argv)
{
	g_socket = netkeyer_init(g_netkeyer_hostaddress, g_netkeyer_port);
	if (g_socket == -1) {
		exit(EXIT_FAILURE);
	}

	/* tests start here, no error handling. TODO (acerion) 2021.12.12: add error handling? */
	 __attribute__((unused)) int result = 0;
	if (argc > 1) {
		result = netkeyer(g_socket, K_DEVICE, argv[1]);
		printf("opening port %s\n", argv[1]);
	}

	printf("first message at initial speed\n");
	result = netkeyer(g_socket, K_MESSAGE, "paris");
	sleep(3);

	printf("speed 40\n");
	result = netkeyer(g_socket, K_SPEED, "40");
	result = netkeyer(g_socket, K_MESSAGE, "paris");
	sleep(2);

	printf("tone 1000, speed 40\n");
	result = netkeyer(g_socket, K_TONE, "1000");
	result = netkeyer(g_socket, K_SPEED, "40");
	result = netkeyer(g_socket, K_MESSAGE, "paris");
	sleep(2);

	printf("tone 800, weight +20\n");
	result = netkeyer(g_socket, K_TONE, "800");
	result = netkeyer(g_socket, K_WEIGHT, "20");
	result = netkeyer(g_socket, K_MESSAGE, "paris");
	sleep(2);

	printf("weight -20\n");
	result = netkeyer(g_socket, K_WEIGHT, "-20");
	result = netkeyer(g_socket, K_MESSAGE, "paris");
	sleep(2);

	printf("weight 0\n");
	result = netkeyer(g_socket, K_WEIGHT, "0");
	printf("speed increase / decrease\n");
	result = netkeyer(g_socket, K_MESSAGE, "p++++++++++aris----------");
	sleep(2);

	printf("half gap\n");
	result = netkeyer(g_socket, K_MESSAGE, "p~ari~s");
	sleep(2);

	printf("tune 3 seconds\n");
	result = netkeyer(g_socket, K_TUNE, "3");
	sleep(4);

	printf("test message abort\n");
	result = netkeyer(g_socket, K_MESSAGE, "paris paris");
	sleep(1);
	result = netkeyer(g_socket, K_ABORT, "");
	sleep(1);

	printf("switch to soundcard\n");
	result = netkeyer(g_socket, K_SDEVICE, "s");
	result = netkeyer(g_socket, K_MESSAGE, "paris");
	sleep(2);

	printf("volume 30\n");
	result = netkeyer(g_socket, K_VOLUME, "30");
	result = netkeyer(g_socket, K_MESSAGE, "paris");
	sleep(2);

	printf("prosigns: SK BK SN AS AR\n");
	result = netkeyer(g_socket, K_MESSAGE, "< > ! & *");
	sleep(4);

	printf("set volume back to 70\n");
	result = netkeyer(g_socket, K_VOLUME, "70");
	result = netkeyer(g_socket, K_MESSAGE, "paris");
	sleep(2);

	printf("back to console\n");
	result = netkeyer(g_socket, K_SDEVICE, "c");
	result = netkeyer(g_socket, K_MESSAGE, "paris");
	sleep(2);

	printf("message with PTT on\n");
	result = netkeyer(g_socket, K_PTT, "1");
	result = netkeyer(g_socket, K_MESSAGE, "paris");
	sleep(2);
	result = netkeyer(g_socket, K_PTT, "0");

	printf("same with different TOD\n");
	result = netkeyer(g_socket, K_TOD, "20");
	result = netkeyer(g_socket, K_PTT, "1");
	result = netkeyer(g_socket, K_MESSAGE, "paris");
	sleep(2);
	result = netkeyer(g_socket, K_PTT, "0");
	result = netkeyer(g_socket, K_TOD, "0");

	/* almost done, reset keyer */
	printf("almost done, reset\n");
	result = netkeyer(g_socket, K_RESET, "");


	printf("test message abort with SIGALRM\n");
	signal(SIGALRM, catchint);
	result = netkeyer(g_socket, K_MESSAGE, "paris paris");
	alarm(2);
	while (1) {}

	printf("done");

	/* end tests */
	if (netkeyer_close(g_socket) == -1) {
		exit(EXIT_FAILURE);
	} else {
		exit(EXIT_SUCCESS);
	}
}


