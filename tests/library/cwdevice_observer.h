#ifndef CWDEVICE_OBSERVER_H
#define CWDEVICE_OBSERVER_H




#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

//#include "server.h"




/**
   Source of information about state of key (key down/key up).

   Structure holding the state of key.

   Structure holding functions that poll the cwdevice, waiting for state of the
   key to change.

   Structure holding a callback that will be called when change to state of
   key has been detected.

   As cwdaemon shows, the other interesting data is the state of PTT pin. The
   structure is not fully ready to support PTT yet.
*/




struct cwdevice_observer_t;
typedef bool (* poll_once_fn_t)(struct cwdevice_observer_t * observer, bool * key_is_down, bool * ptt_is_on);




#define SOURCE_PATH_SIZE (sizeof ("/some/long/path/to/device/used/for/keying"))




/*
  Structure describing pins of tty cwdevice.

  You can assign TIOCM_RTS and TIOCM_DTR values to these pins.
*/
typedef struct {

	/**< Whether to use explicit pin configuration that is specified below,
	   or to allow usage of default values.

	   E.g. code starting cdaemon server process can explicitly specify
	   command-line options for tty lines using values from this struct, or
	   can not specify the options and thus let cwdaemon use implicit,
	   default assignment of the pins.
	 */
	bool explicit;

	unsigned int pin_keying;  /**< Pin of tty port that is used for keying (sending dots and dashes). */
	unsigned int pin_ptt;     /**< Pin of tty port that is used for PTT. */
} tty_pins_t;




/**
   Structure used by test code to configure observer of cwdevice.
*/
typedef struct cwdevice_observer_params_t {
	tty_pins_t tty_pins_config; /* See cwdevice_observer_t::tty_pins_config. */
	char source_path[SOURCE_PATH_SIZE];      /* See cwdevice_observer_t::source_path. */

	bool (* new_ptt_state_cb)(void * arg_ptt_arg, bool ptt_is_on); /**< Callback called on change of ptt pin. */
	void * new_ptt_state_arg;                                      /**< Argument to be passed by cwdaevice observer to new_ptt_state_cb. */
} cwdevice_observer_params_t;




/**
   Methods and data of object that observes cwdaemon's cwdevice
*/
typedef struct cwdevice_observer_t {

	/* User-provided function that opens a specific cwdevice.*/
	bool (* open_fn)(struct cwdevice_observer_t * observer);

	/* User-provided function that closed a specific cwdevice.*/
	void (* close_fn)(struct cwdevice_observer_t * observer);



	/* User-provided callback function that is called by observer each
	   time the state of key pin of cwdevice changes between up and down. */
	bool (* new_key_state_cb)(void * new_key_state_cb_arg, bool key_is_down);

	/*
	  Pointer that will be passed as first argument of new_key_state_cb on
	  each call to the function.

	  This field is set using value of second arg of
	  cwdevice_observer_tty_setup().

	  Since the observer of cwdevice is frequently used to feed data to Morse
	  receiver, this pointer will be set to some cw_easy_receiver_t*
	  variable.
	*/
	void * new_key_state_cb_arg;



	/* User-provided callback function that is called by observer each
	   time the state of PTT pin of cwdevice changes between 'on' and 'off'. */
	bool (* new_ptt_state_cb)(void * ptt_arg, bool ptt_is_on);

	/* Pointer that will be passed as first argument of new_ptt_state_cb
	   on each call to the function. */
	void * new_ptt_state_arg;



	/* At what intervals to poll state of cwdevice? [microseconds]. User
	   should assign KEY_SOURCE_DEFAULT_INTERVAL_US as default value (unless
	   user wants to poll at different interval. */
	unsigned int poll_interval_us;

	/* User-provided function function that checks once, at given moment, if
	   keying pin is down or up, and if ptt pin is on or off. State of keying
	   pin is returned through @p key_is_down. State of ptt pin is returned
	   through @p ptt_is_on */
	poll_once_fn_t poll_once_fn;

	/* Reference to low-level resource related to cwdevice. It may be
	   e.g. a polled file descriptor. To be used by cwdevice-type-specific
	   open/close/poll_once functions. */
	uintptr_t source_reference;

	/* String representation of cwdevice. For regular devices it will
	   be a path (e.g. "/dev/ttyS0"). */
	char source_path[SOURCE_PATH_SIZE];

	/*
	  Low-level integer parameters specifying where in a cwdevice to find
	  information about:

	   - keying. E.g. for ttyS0 it will be a pin/line from which to read key
	     state (cwdaemon uses DTR line by default, but it can be tuned
	     through "-o").

	   - ptt. E.g. for ttyS0 it will be a pin/line from which to read ptt
	     state (cwdaemon uses RTS line by default, but it can be tuned
	     through "-o").
	*/
	tty_pins_t tty_pins_config;

	/* Previous state of key pin, used to recognize when state of key pin changed
	   and when to call "key pin state change" callback.
	   For internal usage only. */
	bool previous_key_is_down;
	/* Previous state of ptt pin, used to recognize when state of ptt pin changed
	   and when to call "ptt pin state change" callback.
	   For internal usage only. */
	bool previous_ptt_is_on;

	/* Flag for internal forever loop in which polling is done.
	   For internal usage only. */
	bool do_polling;

	/* Thread id of thread doing polling in forever loop.
	   For internal usage only. */
	pthread_t thread_id;

	bool thread_created; /**< Whether a thread was created correctly. */
} cwdevice_observer_t;




/**
   Start polling the cwdevice

   @return 0 on success
   @return -1 on failure
*/
int cwdevice_observer_start(cwdevice_observer_t * observer);




/**
   Stop polling the cwdevice
*/
void cwdevice_observer_stop(cwdevice_observer_t * observer);




/**
   @brief Configure cwdevice observer to do periodical polls of the cwdevice

   In theory we can have an observer that learns about key/ptt state changes by
   means other than polling.

   @param observer cwdevice observer to configure
   @param poll_interval_us interval of polling [microseconds]; use 0 to tell function to use default value
   @param poll_once_fn function that executes a single poll every @p poll_interval_us microseconds.
*/
void cwdevice_observer_configure_polling(cwdevice_observer_t * observer, unsigned int poll_interval_us, poll_once_fn_t poll_once_fn);





/**
   @brief Destructor of observer of cwdevice

   Stop any activities of observer. Release all resources used by the
   observer.

   @param [in/out] observer Observer to be destructed

   @return 0 on success
   @return -1 on failure
*/
int cwdevice_observer_dtor(cwdevice_observer_t * observer);




#endif /* #ifndef CWDEVICE_OBSERVER_H */

