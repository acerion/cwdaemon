#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>




/**
   Simple program that checks whether user-space code can access serial port.
*/




int main(int argc, const char * argv[])
{
	if (argc < 2) {
		fprintf(stderr, "[EE] Pass path to the serial device. Call the program like this: %s /dev/ttyS0\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	const char * dev_path = argv[1];

	errno = 0;
	int fd = open(dev_path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "[EE] open(%s): %s / %d\n", dev_path, strerror(errno), errno);
		exit(EXIT_FAILURE);
	}

	errno = 0;
	int state = 0;
	if (0 > ioctl(fd, TIOCMGET, &state)) {
		fprintf(stderr, "[EE] ioctl(%s): %s / %d\n", dev_path, strerror(errno), errno);
		exit(EXIT_FAILURE);
	}

	close(fd);

	fprintf(stderr, "[II] DTR = %d\n", state & TIOCM_DTR);
	fprintf(stderr, "[II] RTS = %d\n", state & TIOCM_RTS);

	exit(EXIT_SUCCESS);
}

