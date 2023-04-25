#ifndef CWDAEMON_LOG
#define CWDAEMON_LOG




#include <stdbool.h>
#include <syslog.h>




/* cwdaemon debug verbosity levels. */
enum cwdaemon_verbosity {
	CWDAEMON_VERBOSITY_N, /* None. Don't display any debug information. */
	CWDAEMON_VERBOSITY_E, /* Error */
	CWDAEMON_VERBOSITY_W, /* Warning */
	CWDAEMON_VERBOSITY_I, /* Info */
	CWDAEMON_VERBOSITY_D  /* Debug */
};




void cwdaemon_debug_open(bool forking);
void cwdaemon_debug_close(void);
void cwdaemon_errmsg(const char * format, ...) __attribute__ ((format (printf, 1, 2)));
void cwdaemon_debug(int verbosity, const char *func, int line, const char *format, ...) __attribute__ ((format (printf, 4, 5)));
const char * log_get_priority_label(int priority);
void log_message(__attribute__((unused)) int priority, const char * format, ...) __attribute__ ((format (printf, 2, 3)));




#endif /* #ifndef CWDAEMON_LOG */

