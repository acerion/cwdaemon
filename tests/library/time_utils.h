#ifndef CWDAEMON_TEST_LIB_TIME_UTILS_H
#define CWDAEMON_TEST_LIB_TIME_UTILS_H



#define _POSIX_C_SOURCE 200809L




#include <time.h>




void timespec_diff(const struct timespec * first, const struct timespec * second, struct timespec * diff);




#endif /* #ifndef CWDAEMON_TEST_LIB_TIME_UTILS_H */

