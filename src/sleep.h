#ifndef CWDAEMON_LIB_SLEEP_H
#define CWDAEMON_LIB_SLEEP_H




/**
   @file

   Sleep functions for cwdaemon

   Three separate functions for microseconds, milliseconds and for seconds.

   Having a dedicated function for each time unit helps me avoid using
   multiplications by constants in client code. Just call proper variant for
   your value specfied in seconds or in microseconds, and don't worry about
   using correct X_PER_Y multiplier.
*/




#define CWDAEMON_MICROSECS_PER_MILLISEC               1000 /* Microseconds in millisecond. */
#define CWDAEMON_MICROSECS_PER_SEC                 1000000 /* Microseconds in second. */
#define CWDAEMON_NANOSECS_PER_MICROSEC                1000 /* Nanoseconds in microsecond. */
#define CWDAEMON_NANOSECS_PER_MILLISEC             1000000 /* Nanoseconds in millisecond. */
#define CWDAEMON_NANOSECS_PER_SEC               1000000000 /* Nanoseconds in second. */




/**
   @brief Non-interruptible micro-seconds-sleep

   Sleep for given value of @p usecs microseconds. Continue the sleep even
   when signal was received by a calling process.

   Interrupts of sleep by signal are not treated as errors.

   @param[in] usecs Microseconds to sleep

   @return 0 if sleep was completed without errors (interrupts by signal may or may not have happened)
   @return -1 on errors
*/
int microsleep_nonintr(unsigned int usecs);




/**
   @brief Non-interruptible milli-seconds-sleep

   Sleep for given value of @p millisecs milliseconds. Continue the sleep
   even when signal was received by a calling process.

   Interrupts of sleep by signal are not treated as errors.

   @param[in] millisecs Milliseconds to sleep

   @return 0 if sleep was completed without errors (interrupts by signal may or may not have happened)
   @return -1 on errors
*/
int millisleep_nonintr(unsigned int millisecs);





/**
   @brief Non-interruptible seconds-sleep

   Sleep for given value of @p secs seconds. Continue the sleep even when
   signal was received by a calling process.

   Interrupts of sleep by signal are not treated as errors.

   @param[in] secs Seconds to sleep

   @return 0 if sleep was completed without errors (interrupts by signal may or may not have happened)
   @return -1 on errors
*/
int sleep_nonintr(unsigned int secs);




#endif /* #ifndef CWDAEMON_LIB_SLEEP_H */

