#ifndef CWDAEMON_KEY_SOURCE_SERIAL_H
#define CWDAEMON_KEY_SOURCE_SERIAL_H




#include <stdbool.h>




#include "key_source.h"




bool key_source_open(cw_key_source_t * source);
void key_source_close(cw_key_source_t * source);




#endif /* #ifndef CWDAEMON_KEY_SOURCE_SERIAL_H */

