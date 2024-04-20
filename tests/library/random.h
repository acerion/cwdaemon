#ifndef CWDAEMON_TESTS_LIB_RANDOM_H
#define CWDAEMON_TESTS_LIB_RANDOM_H




#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>




/**
   @brief Seed a random number generator

   If @p seed is 0, then function chooses some semi-random seed by itself.
   If @p seed is not 0, then the function uses that value to seed the generator.

   @reviewed_on{2024.04.19}

   @param[in] seed If non-zero, use this value as seed

   @return value used to seed random number generator
*/
uint32_t cwdaemon_srandom(uint32_t seed);




/**
   @brief Get random value of type "unsigned int" within specified range

   Values returned through @p result are in range specified by @p lower and
   @p upper (inclusive).

   @reviewed_on{2024.04.19}

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

   @reviewed_on{2024.04.19}

   @param[out] result Generated random value

   @return 0 on success
   @return -1 on failure
*/
int cwdaemon_random_bool(bool * result);




/**
   @brief Get random value of type "bool", but be biased towards returning 'false'

   The random value is returned through @p result.

   The higher the value of @p bias, the more the function is likely to return
   'false' (it's more biased towards returning zero). Keep the value higher
   than zero.

   @reviewed_on{2024.04.19}

   @param[in] bias Bias towards returning 'false'
   @param[out] result Generated random value

   @return 0 on success
   @return -1 on failure
*/
int cwdaemon_random_biased_towards_false(unsigned int bias, bool * result);




/**
   @brief Get an array of random bytes

   Put @p size random bytes into @p buffer.

   @reviewed_on{2024.04.19}

   @param[out] buffer Buffer into which to put random bytes
   @param[out] size Size of the buffer, count of bytes to put into buffer

   @return 0 on success
   @return -1 on failure
*/
int cwdaemon_random_bytes(char * buffer, size_t size);




#endif /* #ifndef CWDAEMON_TESTS_LIB_RANDOM_H */

