#ifndef CWDAEMON_KEY_SOURCE_H
#define CWDAEMON_KEY_SOURCE_H




#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>




struct cw_key_source_t;
typedef bool (* poll_once_fn_t)(struct cw_key_source_t * source, bool * key_is_down);




typedef struct cw_key_source_t {

	/* User-provided function that opens a specific key source.*/
	bool (* open_fn)(struct cw_key_source_t * source);

	/* User-provided function that closed a specific key source.*/
	void (* close_fn)(struct cw_key_source_t * source);

	/* User-provided callback function that is called by key source each
	   time the state of key source changes between up and down. */
	bool (* new_key_state_cb)(void * new_key_state_sink, bool key_is_down);

	/* Pointer that will be passed as first argument of new_key_state_cb
	   on each call to the function. */
	void * new_key_state_sink;

	/* At what intervals to poll key source? [microseconds]. User should
	   assign KEY_SOURCE_DEFAULT_INTERVAL_US as default value (unless
	   user wants to poll at different interval. */
	int poll_interval_us;

	/* User-provided function function that checks once, at given moment,
	   if key is down or up. State of key is returned through @p
	   key_is_down. */
	poll_once_fn_t poll_once_fn;

	/* Reference to low-level resource related to key source. It may be
	   e.g. a polled file descriptor. To be used by source-type-specific
	   open/close/poll_once functions. */
	intptr_t source_reference;

	/* Previous state of key, used to recognize when state of key changed
	   and when to call "key state change" callback.
	   For internal usage only. */
	bool previous_key_is_down;

	/* Flag for internal forever loop in which polling is done.
	   For internal usage only. */
	bool do_polling;

	/* Thread id of thread doing polling in forever loop.
	   For internal usage only. */
	pthread_t thread_id;

} cw_key_source_t;




/**
   Start polling the source
*/
void key_source_start(cw_key_source_t * source);




/**
   Stop polling the source
*/
void key_source_stop(cw_key_source_t * source);




/**
   @brief Configure key source to do periodical polls of the source

   In theory we can have a source that learns about key state changes by
   means other than polling.

   @param source key source to configure
   @param poll_interval_us interval of polling [microseconds]; use 0 to tell function to use default value
   @param poll_once_fn function that executes a single poll every @p poll_interval_us microseconds.
*/
void key_source_configure_polling(cw_key_source_t * source, int poll_interval_us, poll_once_fn_t poll_once_fn);




#endif /* #ifndef CWDAEMON_KEY_SOURCE_H */

