#ifndef CWDAEMON_KEY_SOURCE_H
#define CWDAEMON_KEY_SOURCE_H




#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>




typedef struct cw_key_source_t {
	int poll_interval_us; /* microseconds. */
	unsigned int old_key_state;
	bool (* poll_once)(struct cw_key_source_t * source, unsigned int * key_state);
	bool do_polling;
	pthread_t thread_id;
	bool (* new_key_state_cb)(void * sink, unsigned int arg);
	void * new_key_state_sink;
	intptr_t inner;
} cw_key_source_t;




void key_source_start(cw_key_source_t * source);
void key_source_stop(cw_key_source_t * source);
bool key_source_poll_once(cw_key_source_t * source, unsigned int * key_state);



#endif /* #ifndef CWDAEMON_KEY_SOURCE_H */

