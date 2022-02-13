#ifndef CWDAEMON_TEST_LIB_SOCKET_H
#define CWDAEMON_TEST_LIB_SOCKET_H




#define CWDAEMON_REQUEST_RESET         0
#define CWDAEMON_REQUEST_MESSAGE       1
#define CWDAEMON_REQUEST_SPEED         2
#define CWDAEMON_REQUEST_TONE          3
#define CWDAEMON_REQUEST_ABORT         4
#define CWDAEMON_REQUEST_EXIT          5   /* Tell cwdaemon process to exit cleanly. Formerly known as STOP. */
#define CWDAEMON_REQUEST_WORDMODE      6
#define CWDAEMON_REQUEST_WEIGHT        7
#define CWDAEMON_REQUEST_DEVICE        8
#define CWDAEMON_REQUEST_TOD           9   /* Set txdelay (turn on delay) */
#define CWDAEMON_REQUEST_ADDRESS      10   /* Set port address of device (obsolete) */
#define CWDAEMON_REQUEST_SET14        11   /* Set pin 14 on lpt */
#define CWDAEMON_REQUEST_TUNE         12   /* Tune */
#define CWDAEMON_REQUEST_PTT          13   /* PTT on/off */
#define CWDAEMON_REQUEST_SWITCH       14   /* Set band switch output pins 2,7,8,9 on lpt */
#define CWDAEMON_REQUEST_SDEVICE      15   /* Set sound device */
#define CWDAEMON_REQUEST_VOLUME       16   /* Volume for soundcard */
#define CWDAEMON_REQUEST_REPLY        17   /* Ask cwdaemon to send specified reply after playing text. */




int cwdaemon_socket_connect(const char * address, const char * port);
int cwdaemon_socket_disconnect(int fd);
int cwdaemon_socket_send_request(int fd, int request, const char * value);




#endif /* #ifndef CWDAEMON_TEST_LIB_SOCKET_H */
