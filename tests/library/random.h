#ifndef CWDAEMON_LIB_RANDOM_H
#define CWDAEMON_LIB_RANDOM_H




#include <stdbool.h>
#include <stdint.h>




/**
   @brief Seed a random number generator

   If @p seed is 0, then function chooses some semi-random seed by itself.
   If @p seed is not 0, then the function uses that value to seed the generator.

   @return value used to seed random number generator
*/
uint32_t cwdaemon_srandom(uint32_t seed);




/**
   @brief Get random value of type "unsigned int" within specified range

   Values returned through @p result are in range specified by @p lower and
   @p upper (inclusive).

   @param[in] lower Lower limit on returned values (inclusive)
   @param[in] upper Upper limit on returned values (inclusive)
   @param[out] result Generated random value

   @return 0 on success
   @return -1 on failure
*/
int cwdaemon_random_uint(unsigned int lower, unsigned int upper, unsigned int * result);




/**
   @brief Get random value of type "bool"

   The random value is returned through @p result.

   @param[out] result Generated random value

   @return 0 on success
   @return -1 on failure
*/
int cwdaemon_random_bool(bool * result);




/**
   @brief Get random value of type "bool", but be biased towards returning 'false'

   The random value is returned through @p result.

   The higher the value of @p bias, the more the function is likely to
   return 'false'. Keep the value higher than zero.

   @param[in] bias Bias towards returning 'false'
   @param[out] result Generated random value

   @return 0 on success
   @return -1 on failure
*/
int cwdaemon_random_biased_bool(unsigned int bias, bool * result);




#endif /* #ifndef CWDAEMON_LIB_RANDOM_H */

