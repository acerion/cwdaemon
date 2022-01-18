#define _DEFAULT_SOURCE /* usleep() */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>




#include "key_source.h"




static void * key_source_poll_thread(void * arg_key_source);




void key_source_start(cw_key_source_t * source)
{
	source->do_polling = true;
	pthread_create(&source->thread_id, NULL, key_source_poll_thread, source);
}




void key_source_stop(cw_key_source_t * source)
{
	source->do_polling = false;
	pthread_cancel(source->thread_id);
}




static void * key_source_poll_thread(void * arg_key_source)
{
	cw_key_source_t * source = (cw_key_source_t *) arg_key_source;
	while (source->do_polling) {
		unsigned int key_state = 0;

		if (!source->poll_once(source, &key_state)) {
			fprintf(stderr, "[EE] Failed to poll once\n");
			return NULL;
		}

		/* avoid unnecessary screen updates */
		if (key_state == source->old_key_state) {
			usleep(source->poll_interval_us);
			continue;
		}
		source->old_key_state = key_state;

		source->new_key_state_cb(source->new_key_state_sink, key_state);

		usleep(source->poll_interval_us);
	}

	return NULL;
}




bool key_source_poll_once(cw_key_source_t * source, unsigned int * key_state)
{
	int fd = (int) source->inner;
	errno = 0;
	int status = ioctl(fd, TIOCMGET, key_state);
	if (status != 0) {
		char buf[32] = { 0 };
		strerror_r(errno, buf, sizeof (buf));
		fprintf(stderr, "[EE]: ioctl(TIOCMGET): %s\n", buf);
		return false;
	}
	return true;
}


