#ifndef CWDAEMON_TEST_LIB_LOG_H
#define CWDAEMON_TEST_LIB_LOG_H




#define test_log_newline()           fprintf(stderr, "\n")
#define test_log_debug(format, ...)  fprintf(stderr, "[DD] " format, __VA_ARGS__)
#define test_log_info(format, ...)   fprintf(stderr, "[II] " format, __VA_ARGS__)
#define test_log_notice(format, ...) fprintf(stderr, "[NN] " format, __VA_ARGS__)
#define test_log_warn(format, ...)   fprintf(stderr, "[WW] " format, __VA_ARGS__)
#define test_log_err(format, ...)    fprintf(stderr, "[EE] " format, __VA_ARGS__)




#endif /* #ifndef CWDAEMON_TEST_LIB_LOG_H */

