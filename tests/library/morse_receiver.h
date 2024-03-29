#ifndef CWDAEMON_TEST_LIB_MORSE_RECEIVER_H
#define CWDAEMON_TEST_LIB_MORSE_RECEIVER_H




#include "cw_easy_receiver.h"
#include "cwdevice_observer.h"
#include "events.h"
#include "misc.h"
#include "thread.h"




typedef struct morse_receiver_config_t {
	tty_pins_t observer_tty_pins_config;
	int wpm;                                 /**< Receiver speed (words-per-minute), for receiver working in non-adaptive mode (which is usually the case). */
} morse_receiver_config_t;




typedef struct morse_receiver_t {
	thread_t thread;
	morse_receiver_config_t config;

	/** Reference to test's events container. Used to collect events
	    registered during test by Morse receiver. */
	events_t * events;
} morse_receiver_t;




int morse_receiver_ctor(const morse_receiver_config_t * config, morse_receiver_t * receiver);
void morse_receiver_dtor(morse_receiver_t * receiver);
int morse_receiver_start(morse_receiver_t * receiver);
int morse_receiver_wait(morse_receiver_t * receiver);



#endif /* #ifndef CWDAEMON_TEST_LIB_MORSE_RECEIVER_H */

