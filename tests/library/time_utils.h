#ifndef CWDAEMON_TESTS_LIB_TIME_UTILS_H
#define CWDAEMON_TESTS_LIB_TIME_UTILS_H




#include <time.h>




#define TESTS_MICROSECS_PER_MILLISEC               1000 /* Microseconds in millisecond. */
#define TESTS_MICROSECS_PER_SEC                 1000000 /* Microseconds in second. */
#define TESTS_NANOSECS_PER_MICROSEC                1000 /* Nanoseconds in microsecond. */
#define TESTS_NANOSECS_PER_SEC               1000000000 /* Nanoseconds in second. */




/// @brief Get difference between two time stamps
///
/// Get difference between an earlier timestamp @p first and later timestamp
/// @p second. Put the difference in @p diff.
///
/// Caller must make sure that @p first occurred before @p second, otherwise
/// the result will be incorrect.
///
/// @reviewed_on{2024.04.20}
///
/// @param[in] first First (earlier) timestamp
/// @param[in] second Second (later) timestamp
/// @param[out] diff The difference between @p first and @p second
void timespec_diff(const struct timespec * first, const struct timespec * second, struct timespec * diff);




#endif /* #ifndef CWDAEMON_TESTS_LIB_TIME_UTILS_H */

