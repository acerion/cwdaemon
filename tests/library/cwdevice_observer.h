#ifndef CW_KEY_SOURCE_H
#define CW_KEY_SOURCE_H




#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>




/**
   Source of information about state of key (key down/key up).

   Structure holding the state of key.

   Structure holding functions that poll the source, waiting for state of the
   key to change.

   Structure holding a callback that will be called when change to state of
   key has been detected.

   As cwdaemon shows, the other interesting data is the state of PTT pin. The
   structure is not fully ready to support PTT yet.
*/




struct cwdevice_observer_t;
typedef bool (* poll_once_fn_t)(struct cwdevice_observer_t * source, bool * key_is_down, bool * ptt_is_on);




#define SOURCE_PATH_SIZE (sizeof ("/some/long/path/to/device/used/for/keying"))




/**
   Structure used by test code to configure observer of cwdevice.
*/
typedef struct cwdevice_observer_params_t {
	unsigned int param_keying; /* See cwdevice_observer_t::param_keying. */
	unsigned int param_ptt;    /* See cwdevice_observer_t::param_ptt. */
	char source_path[SOURCE_PATH_SIZE];      /* See key_source_t::source_path. */

	bool (* new_ptt_state_cb)(void * arg_ptt_arg, bool ptt_is_on); /**< Callback called on change of ptt pin. */
	void * new_ptt_state_arg;                                      /**< Argument to be passed by cwdaevice observer to new_ptt_state_cb. */
} cwdevice_observer_params_t;




/**
   Methods and data of object that observes cwdaemon's cwdevice
*/
typedef struct cwdevice_observer_t {

	/* User-provided function that opens a specific key source.*/
	bool (* open_fn)(struct cwdevice_observer_t * source);

	/* User-provided function that closed a specific key source.*/
	void (* close_fn)(struct cwdevice_observer_t * source);

	/* User-provided callback function that is called by key source each
	   time the state of key source changes between up and down. */
	bool (* new_key_state_cb)(void * new_key_state_sink, bool key_is_down);

	/* User-provided callback function that is called by key source each
	   time the state of PTT value changes between 'on' and 'off'. */
	bool (* new_ptt_state_cb)(void * ptt_arg, bool ptt_is_on);

	/* Pointer that will be passed as first argument of new_key_state_cb
	   on each call to the function. */
	void * new_key_state_sink;

	/* Pointer that will be passed as first argument of new_ptt_state_cb
	   on each call to the function. */
	void * new_ptt_state_arg;

	/* At what intervals to poll key source? [microseconds]. User should
	   assign KEY_SOURCE_DEFAULT_INTERVAL_US as default value (unless
	   user wants to poll at different interval. */
	unsigned int poll_interval_us;

	/* User-provided function function that checks once, at given moment,
	   if key is down or up, and if ptt is on or off. State of key is
	   returned through @p key_is_down. State of ptt is returned through
	   @p ptt_is_on */
	poll_once_fn_t poll_once_fn;

	/* Reference to low-level resource related to key source. It may be
	   e.g. a polled file descriptor. To be used by source-type-specific
	   open/close/poll_once functions. */
	uintptr_t source_reference;

	/* String representation of key source. For regular devices it will
	   be a path (e.g. "/dev/ttyS0"). */
	char source_path[SOURCE_PATH_SIZE];

	/* Low-level integer parameter specifying where in a keying source to
	   find information about keying. E.g. for ttyS0 it will be a
	   pin/line from which to read key state (cwdaemon uses DTR line by
	   default, but it can be tuned through "-o"). */
	unsigned int param_keying;

	/* Low-level integer parameter specifying where in a keying source to
	   find information about ptt. E.g. for ttyS0 it will be a
	   pin/line from which to read ptt state (cwdaemon uses RTS line by
	   default, but it can be tuned through "-o"). */
	unsigned int param_ptt;

	/* Previous state of key, used to recognize when state of key changed
	   and when to call "key state change" callback.
	   For internal usage only. */
	bool previous_key_is_down;
	/* Previous state of ptt, used to recognize when state of ptt changed
	   and when to call "ptt state change" callback.
	   For internal usage only. */
	bool previous_ptt_is_on;

	/* Flag for internal forever loop in which polling is done.
	   For internal usage only. */
	bool do_polling;

	/* Thread id of thread doing polling in forever loop.
	   For internal usage only. */
	pthread_t thread_id;

} cwdevice_observer_t;




/**
   Start polling the source
*/
void cw_key_source_start(cwdevice_observer_t * source);




/**
   Stop polling the source
*/
void cw_key_source_stop(cwdevice_observer_t * source);




/**
   @brief Configure key source to do periodical polls of the source

   In theory we can have a source that learns about key state changes by
   means other than polling.

   @param source key source to configure
   @param poll_interval_us interval of polling [microseconds]; use 0 to tell function to use default value
   @param poll_once_fn function that executes a single poll every @p poll_interval_us microseconds.
*/
void cw_key_source_configure_polling(cwdevice_observer_t * source, unsigned int poll_interval_us, poll_once_fn_t poll_once_fn);




#endif /* #ifndef CW_KEY_SOURCE_H */

