#ifndef CWDAEMON_LIB_SLEEP_H
#define CWDAEMON_LIB_SLEEP_H




#include <stdbool.h>




#define CWDAEMON_MICROSECS_PER_MILLISEC               1000 /* Microseconds in millisecond. */
#define CWDAEMON_MICROSECS_PER_SEC                 1000000 /* Microseconds in second. */
#define CWDAEMON_NANOSECS_PER_MICROSEC                1000 /* Nanoseconds in microsecond. */
#define CWDAEMON_NANOSECS_PER_SEC               1000000000 /* Nanoseconds in second. */




/**
   Sleep functions for cwdaemon

   Three separate functions for microseconds, milliseconds and for seconds.

   Having a dedicated function for each time unit helps me avoid using
   multiplications by constants in client code. Just call proper variant for
   your value specfied in seconds or in microseconds, and don't worry about
   using correct X_PER_Y multiplier.
*/




/**
   @brief Non-interruptible micro-seconds-sleep

   Sleep for given value of @p usecs microseconds. Notify caller through
   return value if any signal tried to interrupt the sleep (but function
   continues to sleep for entire duration of @p usecs microseconds).

   @param[in] usecs Microseconds to sleep

   @return 0 if sleep was completed without errors (interrupts by signal may or may not have happened)
   @return -1 on errors (interrupts by signal are not treated as errors)
*/
int microsleep_nonintr(unsigned int usecs);




/**
   @brief Non-interruptible milli-seconds-sleep

   Sleep for given value of @p millisecs milliseconds. Notify caller through return value
   if any signal tried to interrupt the sleep (but function continues to
   sleep for entire duration of @p millisecs milliseconds).

   @param[in] millisecs Milliseconds to sleep

   @return 0 if sleep was completed without errors (interrupts by signal may or may not have happened)
   @return -1 on errors (interrupts by signal are not treated as errors)
*/
int millisleep_nonintr(unsigned int millisecs);





/**
   @brief Non-interruptible seconds-sleep

   Sleep for given value of @p secs seconds. Notify caller through return
   value if any signal tried to interrupt the sleep (but function continues
   to sleep for entire duration of @p secs seconds).

   @param[in] secs Seconds to sleep

   @return 0 if sleep was completed without errors (interrupts by signal may or may not have happened)
   @return -1 on errors (interrupts by signal are not treated as errors)
*/
int sleep_nonintr(unsigned int secs);




#endif /* #ifndef CWDAEMON_LIB_SLEEP_H */

