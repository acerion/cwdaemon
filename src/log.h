#ifndef CWDAEMON_LOG
#define CWDAEMON_LOG




/* cwdaemon debug verbosity levels. */
enum cwdaemon_verbosity {
	CWDAEMON_VERBOSITY_N, /* None. Don't display any debug information. */
	CWDAEMON_VERBOSITY_E, /* Error */
	CWDAEMON_VERBOSITY_W, /* Warning */
	CWDAEMON_VERBOSITY_I, /* Info */
	CWDAEMON_VERBOSITY_D  /* Debug */
};




const char * log_get_priority_label(int priority);




#endif /* #ifndef CWDAEMON_LOG */

