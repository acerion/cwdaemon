#ifndef CW_CWDEVICE_OBSERVER_SERIAL_H
#define CW_CWDEVICE_OBSERVER_SERIAL_H




#include <stdbool.h>
#include <sys/ioctl.h> /* For those users of the cwdevice that need to specify TIOCM_XXX tty pin. */




#include "cwdevice_observer.h"




/**
   Implementation of cwdevice_observer_t::open_fn function specific to serial line file
*/
bool cwdevice_observer_serial_open(cwdevice_observer_t * observer);




/**
   Implementation of cwdevice_observer_t::close_fn function specific to serial line file
*/
void cwdevice_observer_serial_close(cwdevice_observer_t * observer);




/**
   Implementation of cwdevice_observer_t::poll_once_fn function specific to serial line file
*/
bool cwdevice_observer_serial_poll_once(cwdevice_observer_t * observer, bool * key_is_down, bool * ptt_is_on);




int cwdevice_observer_tty_setup(cwdevice_observer_t * observer, void * key_state_cb_arg, const tty_pins_t * observer_pins_config);




#endif /* #ifndef CW_CWDEVICE_OBSERVER_SERIAL_H */

