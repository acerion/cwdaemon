#ifndef CWDAEMON_TESTS_LIB_MORSE_RECEIVER_H
#define CWDAEMON_TESTS_LIB_MORSE_RECEIVER_H




#include "tests/library/cw_easy_receiver.h"
#include "tests/library/cwdevice_observer.h"
#include "tests/library/events.h"
#include "tests/library/misc.h"
#include "tests/library/thread.h"




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

	/// @brief Observer of cwdevice on which Morse code is being keyed by cwdaemon.
	cwdevice_observer_t cwdevice_observer;

	/// @brief The low-level component that is decoding Morse code.
	cw_easy_receiver_t libcw_receiver;
} morse_receiver_t;




/// @brief Configure new Morse receiver
///
/// Set some members of @p receiver necessary for next steps of usage of the
/// receiver. @p receiver must be allocated by caller.
///
/// @reviewed_on{2024.04.21}
///
/// @param[in] config Configuration to be used for the receiver
/// @param receiver Receiver to be constructed/configured
///
/// @return 0
int morse_receiver_configure(morse_receiver_config_t const * config, morse_receiver_t * receiver);




/// @brief Deconfigure a receiver
///
/// @reviewed_on{2024.04.21}
///
/// @param receiver Receiver to be deconfigured
void morse_receiver_deconfigure(morse_receiver_t * receiver);




/// @brief Start a Morse receiver
///
/// @reviewed_on{2024.04.21}
///
/// @param receiver Morse receiver to start
///
/// @return 0 on success
/// @return -1 on failure
int morse_receiver_start(morse_receiver_t * receiver);




/// @brief Wait for end of receiving process
///
/// In certain condition the receiver will stop receiving and will stop
/// operating. This function is waiting for the stop and will return when
/// receiver's stop is detected.
///
/// The condition for the stop is lack of keying on cwdevice for N seconds.
///
/// @reviewed_on{2024.04.21}
///
/// @param receiver Receiver on which to wait
///
/// @return 0
int morse_receiver_wait_for_stop(morse_receiver_t * receiver);




#endif /* #ifndef CWDAEMON_TESTS_LIB_MORSE_RECEIVER_H */

