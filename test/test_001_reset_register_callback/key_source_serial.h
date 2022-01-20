#ifndef CWDAEMON_KEY_SOURCE_SERIAL_H
#define CWDAEMON_KEY_SOURCE_SERIAL_H




#include <stdbool.h>




#include "key_source.h"




/**
   Implementation of cw_key_source_t::open_fn function specific to serial line file
*/
bool key_source_serial_open(cw_key_source_t * source);




/**
   Implementation of cw_key_source_t::close_fn function specific to serial line file
*/
void key_source_serial_close(cw_key_source_t * source);




/**
   Implementation of cw_key_source_t::poll_once_fn function specific to serial line file
*/
bool key_source_serial_poll_once(cw_key_source_t * source, bool * key_is_down);




#endif /* #ifndef CWDAEMON_KEY_SOURCE_SERIAL_H */

