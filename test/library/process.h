#ifndef CWDAEMON_TEST_LIB_PROCESS_H
#define CWDAEMON_TEST_LIB_PROCESS_H




#include <sys/types.h> /* pid_t */




typedef struct cwdaemon_process_t {
	int fd;
	pid_t pid;
} cwdaemon_process_t;




#endif /* #ifndef CWDAEMON_TEST_LIB_PROCESS_H */


