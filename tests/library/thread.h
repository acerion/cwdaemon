#ifndef CWDAEMON_TEST_LIB_THREAD_H
#define CWDAEMON_TEST_LIB_THREAD_H




#include <pthread.h>




/*
  Wrappers around pthread functions and data structures.
*/




typedef enum {
	thread_not_started,    /**< Thread has not been started yet. */
	thread_running,        /**< Thread is still running. */
	thread_stopped_err,    /**< Thread completed with errors. */
	thread_stopped_ok,     /**< Thread completed successfully. */
} thread_status_t;




typedef struct thread_t {
	const char * name;             /**< Human-readable label for logs. */

	void * (* thread_fn)(void *);  /**< The main thread function. */
	void * thread_fn_arg;          /**< Argument to the main thread function. */

	pthread_t thread_id;
	pthread_attr_t thread_attr;

	thread_status_t status;        /**< Current status of thread. Set by thread, read by client code. */
} thread_t;




/**
   @brief Start a given thread

   @param thread Preallocated thread structure

   @return 0 on success
   @return -1 on error
 */
int thread_start(thread_t * thread);




/**
   @brief Stop a given thread

   @param thread Thread to stop

   @return 0 on success
   @return -1 on error
 */
int thread_join(thread_t * thread);




/**
   @brief Clean up thread data structure after a thread has been stopped

   @param thread Thread to clean up after it has been stopped

   @return 0 on success
   @return -1 on error
 */
int thread_cleanup(thread_t * thread);




#endif /* #ifndef CWDAEMON_TEST_LIB_THREAD_H */

