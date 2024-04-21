#ifndef CWDAEMON_LIB_SLEEP_H
#define CWDAEMON_LIB_SLEEP_H




#include <stdbool.h>




/**
   @file

   Sleep functions for cwdaemon

   Three separate functions for microseconds, milliseconds and for seconds.

   Having a dedicated function for each time unit helps me avoid using
   multiplications by constants in client code. Just call proper variant for
   your value specfied in seconds or in microseconds, and don't worry about
   using correct X_PER_Y multiplier.
*/




/**
   @brief Non-interruptible micro-seconds-sleep

   Sleep for given value of @p usecs microseconds.

   @reviewed_on{2024.04.17}

   @param[in] usecs Microseconds to sleep

   @return 0 if sleep was completed without errors (interrupts by EINTR signal may or may not have happened)
   @return -1 on errors (interrupts by EINTR signal are not treated as errors)
*/
int test_microsleep_nonintr(unsigned int usecs);




/**
   @brief Non-interruptible milli-seconds-sleep

   Sleep for given value of @p millisecs milliseconds.

   @reviewed_on{2024.04.17}

   @param[in] millisecs Milliseconds to sleep

   @return 0 if sleep was completed without errors (interrupts by EINTR signal may or may not have happened)
   @return -1 on errors (interrupts by EINTR signal are not treated as errors)
*/
int test_millisleep_nonintr(unsigned int millisecs);





/**
   @brief Non-interruptible seconds-sleep

   Sleep for given value of @p secs seconds.

   @reviewed_on{2024.04.17}

   @param[in] secs Seconds to sleep

   @return 0 if sleep was completed without errors (interrupts by EINTR signal may or may not have happened)
   @return -1 on errors (interrupts by EINTR signal are not treated as errors)
*/
int test_sleep_nonintr(unsigned int secs);




#endif /* #ifndef CWDAEMON_LIB_SLEEP_H */

