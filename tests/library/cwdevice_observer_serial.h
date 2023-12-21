#ifndef CW_KEY_SOURCE_SERIAL_H
#define CW_KEY_SOURCE_SERIAL_H




#include <stdbool.h>
#include <sys/ioctl.h> /* For those users of the key source that need to specify TIOCM_XXX tty pin. */




#include "cwdevice_observer.h"




/**
   Implementation of cwdevice_observer_t::open_fn function specific to serial line file
*/
bool cw_key_source_serial_open(cwdevice_observer_t * observer);




/**
   Implementation of cwdevice_observer_t::close_fn function specific to serial line file
*/
void cw_key_source_serial_close(cwdevice_observer_t * observer);




/**
   Implementation of cwdevice_observer_t::poll_once_fn function specific to serial line file
*/
bool cw_key_source_serial_poll_once(cwdevice_observer_t * observer, bool * key_is_down, bool * ptt_is_on);




#endif /* #ifndef CW_KEY_SOURCE_SERIAL_H */

