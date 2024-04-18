#ifndef CW_CWDEVICE_OBSERVER_SERIAL_H
#define CW_CWDEVICE_OBSERVER_SERIAL_H




#include <stdbool.h>
#include <sys/ioctl.h> /* For those users of the cwdevice that need to specify TIOCM_XXX tty pin. */




#include "cwdevice_observer.h"




/// @brief Implementation of cwdevice_observer_t::open_fn function specific to serial line file
///
/// @reviewed_on{2024.04.17}
///
/// @param observer cwdevice observer that should access an opened cwdevice
///
/// @return 0 on success
/// @return -1 on failure
int cwdevice_observer_serial_open(cwdevice_observer_t * observer);




/// @brief Implementation of cwdevice_observer_t::close_fn function specific to serial line file
///
/// @reviewed_on{2024.04.17}
///
/// @param observer cwdevice observer in which we have a cwdevice to close
///
/// @return 0 on success
/// @return -1 on failure
void cwdevice_observer_serial_close(cwdevice_observer_t * observer);




/// @brief Implementation of cwdevice_observer_t::poll_once_fn function specific to serial line file
///
/// @reviewed_on{2024.04.17}
///
/// @param observer cwdevice observer observing a tty device
/// @param[out] key_is_down whether keying pin is down
/// @param[out] ptt_is_down whether ptt pin is down
///
/// @return 0 on success
/// @return -1 on failure
int cwdevice_observer_serial_poll_once(cwdevice_observer_t * observer, bool * key_is_down, bool * ptt_is_on);




/// @brief Configure given observer to be used with tty device
///
/// If @p observer_pins_config is NULL, the observer will use default
/// assignment of functions (key, ptt) to tty's pins.
///
/// @reviewed_on{2024.04.17}
///
/// @param observer cwdevice observer to be configured
/// @param[in] observer_pins_config Configuration of pins on the cwdevice
///
/// @return 0 on success
/// @return -1 on failure
int cwdevice_observer_tty_setup(cwdevice_observer_t * observer, const tty_pins_t * observer_pins_config);




#endif /* #ifndef CW_CWDEVICE_OBSERVER_SERIAL_H */

