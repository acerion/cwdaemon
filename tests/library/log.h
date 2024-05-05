#ifndef CWDAEMON_TEST_LIB_LOG_H
#define CWDAEMON_TEST_LIB_LOG_H




#include <stdio.h>
#include <syslog.h>




#define test_log_newline()           fprintf(stderr, "\n")
#define test_log_debug(format, ...)  fprintf(stderr, "[DD] " format, __VA_ARGS__)
#define test_log_info(format, ...)   fprintf(stderr, "[II] " format, __VA_ARGS__)
#define test_log_notice(format, ...) fprintf(stderr, "[NN] " format, __VA_ARGS__)
#define test_log_warn(format, ...)   fprintf(stderr, "[WW] " format, __VA_ARGS__)
#define test_log_err(format, ...)    fprintf(stderr, "[EE] " format, __VA_ARGS__)




/// @brief Log message to something more persistent than stdout/stderr
///
/// Sometimes past info in console may be erased by printing random bytes to
/// the console. This function allows us to save really important info to
/// some more persistent location.
///
/// @param[in] priority syslog priority level enum (LOG_ERR, LOG_INFO and friends)
void test_log_persistent(int priority, const char * format, ...) __attribute__ ((format (printf, 2, 3)));




#endif /* #ifndef CWDAEMON_TEST_LIB_LOG_H */

