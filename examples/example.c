/*
 * example.c - example program for cwdaemon
 * Copyright (C) 2003, 2006 Joop Stakenborg <pg4i@amsat.org>
 * Copyright (C) 2012 - 2015 Kamil Ignacak <acerion@wp.pl>
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

#define _POSIX_SOURCE /* getaddrinfo() and friends. */

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

#define K_MESSAGE 1

#define K_RESET 0
#define K_SPEED 2
#define K_TONE 3
#define K_ABORT 4
#define K_STOP 5
#define K_WORDMODE 6
#define K_WEIGHT 7
#define K_DEVICE 8
#define K_TOD 9         // set txdelay (turn on delay)
#define K_ADDRESS 10    // set port address of device (obsolete)
#define K_SET14 11      // set pin 14 on lpt
#define K_TUNE 12       // tune
#define K_PTT 13        // PTT on/off
#define K_SWITCH 14     // set band switch output pins 2,7,8,9 on lpt
#define K_SDEVICE 15	// set sound device
#define K_VOLUME 16     // volume for soundcard

static char netkeyer_port[] = "6789";
static char netkeyer_hostaddress[16] = "127.0.0.1";
static int socket_descriptor;
static struct addrinfo *addrinfo_list;
static struct addrinfo *selected_addrinfo;

int netkeyer_init(void);
int netkeyer_close(void);
int netkeyer(int cw_op, const char *cwmessage);
void catchint(int signal);





int netkeyer_init(void)
{
	/* Code in this function has been copied from
	   getaddrinfo() man page. */

	struct addrinfo hints;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = IPPROTO_UDP;

	int rv = getaddrinfo(netkeyer_hostaddress,
			     netkeyer_port,
			     &hints, &addrinfo_list);
	if (rv) {
		fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(rv));
		return -1;
	}

	/* getaddrinfo() returns a list of address structures.
	   Try each address until we successfully connect(2).
	   If socket(2) (or connect(2)) fails, we (close the socket
	   and) try the next address. */

	for (selected_addrinfo = addrinfo_list;
	     selected_addrinfo;
	     selected_addrinfo = selected_addrinfo->ai_next) {

		socket_descriptor = socket(selected_addrinfo->ai_family,
					   selected_addrinfo->ai_socktype,
					   selected_addrinfo->ai_protocol);
		if (socket_descriptor == -1) {
			continue;
		}

		if (connect(socket_descriptor,
			    selected_addrinfo->ai_addr,
			    selected_addrinfo->ai_addrlen) != -1) {
			break;
		}

		close(socket_descriptor);
	}

	if (!selected_addrinfo) {
		fprintf(stderr, "Could not connect\n");
		return -1;
	} else {
		return 0;
	}
}





int netkeyer_close(void)
{
	if (close(socket_descriptor) == -1) {
		perror("close call failed");
		return -1;
	}

	freeaddrinfo(addrinfo_list);           /* No longer needed */

	return 0;
}





int netkeyer(int cw_op, const char *cwmessage)
{
	char buf[80];

	switch (cw_op) {
		case K_RESET :
  			buf[0] = 27;
			sprintf(buf + 1, "0");
 			break;
		case K_MESSAGE :
			sprintf(buf, "%s", cwmessage);
 			break;
		case K_SPEED :
  			buf[0] = 27;
			sprintf(buf + 1, "2");
			sprintf(buf + 2, "%s", cwmessage);
 			break;
		case K_TONE :
  			buf[0] = 27;
			sprintf(buf + 1, "3");
			sprintf(buf + 2, "%s", cwmessage);
 			break;
		case K_ABORT :
  			buf[0] = 27;
			sprintf(buf + 1, "4");
 			break;
		case K_STOP :
  			buf[0] = 27;
			sprintf(buf + 1, "5");
 			break;
		case K_WORDMODE :
  			buf[0] = 27;
			sprintf(buf + 1, "6");
 			break;
		case K_WEIGHT :
  			buf[0] = 27;
			sprintf(buf + 1, "7");
			sprintf(buf + 2, "%s", cwmessage);
 			break;
		case K_DEVICE :
  			buf[0] = 27;
			sprintf(buf + 1, "8");
			sprintf(buf + 2, "%s", cwmessage);
 			break;
		case K_PTT :
  			buf[0] = 27;
			sprintf(buf + 1, "a");
			sprintf(buf + 2, "%s", cwmessage);
 			break;
		case K_TUNE :
  			buf[0] = 27;
			sprintf(buf + 1, "c");
			sprintf(buf + 2, "%s", cwmessage);
 			break;
		case K_TOD :
  			buf[0] = 27;
			sprintf(buf + 1, "d");
			sprintf(buf + 2, "%s", cwmessage);
 			break;
		case K_SDEVICE :
  			buf[0] = 27;
			sprintf(buf + 1, "f");
			sprintf(buf + 2, "%s", cwmessage);
 			break;
		case K_VOLUME :
  			buf[0] = 27;
			sprintf(buf + 1, "g");
			sprintf(buf + 2, "%s", cwmessage);
 			break;
		default :
			buf[0] = '\0';
	}

	ssize_t sendto_rc = 0;
	if (buf[0] != '\0') {
		sendto_rc = sendto(socket_descriptor,
				   buf, sizeof (buf),
				   0,
				   selected_addrinfo->ai_addr,
				   sizeof (struct addrinfo));
	}

	buf[0] = '\0';
	cw_op = K_RESET;

	if (sendto_rc == -1) {
     	 	printf("Keyer send failed (%s)!\n", strerror(errno));
		return -1;
     	} else {
		return 0;
	}
}





void catchint(__attribute__((unused)) int signal)
{
	__attribute__((unused)) int result = netkeyer(K_ABORT, "");
	exit(EXIT_SUCCESS);
}





int main(int argc, char **argv)
{
	if (netkeyer_init() == -1) {
		exit(EXIT_FAILURE);
	}

	/* tests start here, no error handling */
	int result;
	if (argc > 1) {
		result = netkeyer(K_DEVICE, argv[1]);
		printf("opening port %s\n", argv[1]);
	}

	printf("first message at initial speed\n");
	result = netkeyer(K_MESSAGE, "paris");
	sleep(3);

	printf("speed 40\n");
	result = netkeyer(K_SPEED, "40");
	result = netkeyer(K_MESSAGE, "paris");
	sleep(2);

	printf("tone 1000, speed 40\n");
	result = netkeyer(K_TONE, "1000");
	result = netkeyer(K_SPEED, "40");
	result = netkeyer(K_MESSAGE, "paris");
	sleep(2);

	printf("tone 800, weight +20\n");
	result = netkeyer(K_TONE, "800");
	result = netkeyer(K_WEIGHT, "20");
	result = netkeyer(K_MESSAGE, "paris");
	sleep(2);

	printf("weight -20\n");
	result = netkeyer(K_WEIGHT, "-20");
	result = netkeyer(K_MESSAGE, "paris");
	sleep(2);

	printf("weight 0\n");
	result = netkeyer(K_WEIGHT, "0");
	printf("speed increase / decrease\n");
	result = netkeyer(K_MESSAGE, "p++++++++++aris----------");
	sleep(2);

	printf("half gap\n");
	result = netkeyer(K_MESSAGE, "p~ari~s");
	sleep(2);

	printf("tune 3 seconds\n");
	result = netkeyer(K_TUNE, "3");
	sleep(4);

	printf("test message abort\n");
	result = netkeyer(K_MESSAGE, "paris paris");
	sleep(1.2);
	result = netkeyer(K_ABORT, "");
	sleep(1);

	printf("switch to soundcard\n");
	result = netkeyer(K_SDEVICE, "s");
	result = netkeyer(K_MESSAGE, "paris");
	sleep(2);

	printf("volume 30\n");
	result = netkeyer(K_VOLUME, "30");
	result = netkeyer(K_MESSAGE, "paris");
	sleep(2);

	printf("prosigns: SK BK SN AS AR\n");
	result = netkeyer(K_MESSAGE, "< > ! & *");
	sleep(4);

	printf("set volume back to 70\n");
	result = netkeyer(K_VOLUME, "70");
	result = netkeyer(K_MESSAGE, "paris");
	sleep(2);

	printf("back to console\n");
	result = netkeyer(K_SDEVICE, "c");
	result = netkeyer(K_MESSAGE, "paris");
	sleep(2);

	printf("message with PTT on\n");
	result = netkeyer(K_PTT, "1");
	result = netkeyer(K_MESSAGE, "paris");
	sleep(2);
	result = netkeyer(K_PTT, "");

	printf("same with different TOD\n");
	result = netkeyer(K_TOD, "20");
	result = netkeyer(K_PTT, "1");
	result = netkeyer(K_MESSAGE, "paris");
	sleep(2);
	result = netkeyer(K_PTT, "");
	result = netkeyer(K_TOD, "0");

	/* almost done, reset keyer */
	printf("almost done, reset\n");
	result = netkeyer(K_RESET, "");


	printf("test message abort with SIGALRM");
	signal(SIGALRM, catchint);
	result = netkeyer(K_MESSAGE, "paris paris");
	alarm(2);
	while (1) {}

	printf("done");

	/* end tests */
	if (netkeyer_close() == -1) {
		exit(EXIT_FAILURE);
	} else {
		exit(EXIT_SUCCESS);
	}
}
