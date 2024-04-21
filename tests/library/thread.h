#ifndef CWDAEMON_TESTS_LIB_THREAD_H
#define CWDAEMON_TESTS_LIB_THREAD_H




#include <pthread.h>
#include <stdbool.h>




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

	bool thread_loop_continue;     /**< Flag controlling whether a loop inside of thread should be running. */
} thread_t;




/**
   @brief Start a given thread

   @reviewed_on{2024.04.20}

   @param thread Preallocated thread structure

   @return 0 on success
   @return -1 on error
 */
int thread_start(thread_t * thread);




/**
   @brief Clean up thread data structure after a thread has been stopped

   @reviewed_on{2024.04.20}

   @param thread Thread to clean up after it has been stopped

   @return 0 on success
   @return -1 on error
 */
int thread_dtor(thread_t * thread);




#endif /* #ifndef CWDAEMON_TESTS_LIB_THREAD_H */

