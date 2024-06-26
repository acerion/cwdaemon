#ifndef CWDAEMON_TEST_LIB_EVENTS_H
#define CWDAEMON_TEST_LIB_EVENTS_H




#include <pthread.h>
#include <time.h>

#include "misc.h"
#include "test_defines.h"




typedef enum {
	etype_none = 0,         /**< Indicates empty/invalid event. */
	etype_morse,            /**< Something was received as Morse code by Morse receiver observing a keying pin on cwdevice. */
	etype_reply,            /**< A reply was received by cwdaemon client over network socket from cwdaemon server. */
	etype_req_exit,         /**< EXIT Escape request has been sent by test program to tested cwdaemon server. */
	etype_sigchld,          /**< SIGCHLD signal was received from child process. For now the child process is always the local test instance of cwdaemon, started by test program. */
} event_type_t;




typedef struct {
	char string[MORSE_RECV_BUFFER_SIZE];
} event_morse_receive_t;




/*
  This structe doesn't store a string. It stores an array of bytes with
  explicit count of bytes.

  Bytes will be sent through send().
  Count of bytes will be passed to send().
*/
typedef struct test_request_t {
	size_t n_bytes;                         /**< How many bytes to send? */
	char bytes[CLIENT_SEND_BUFFER_SIZE];    /**< What exactly bytes do we want to send? */
} test_request_t;



/*
  This structe doesn't store a string. It stores an array of bytes with explicit count of bytes.

  Bytes are received through recv().
  Count of bytes is a value returned by recv().
*/
typedef struct test_reply_data_t {
	size_t n_bytes;                        /**< How many bytes we expect to receive send? */
	char bytes[CLIENT_RECV_BUFFER_SIZE];   /**< What exactly bytes do we expect to receive? */
} test_reply_data_t;




// Struct is packed to allow usage of memcmp(). See
// https://wiki.sei.cmu.edu/confluence/display/c/EXP42-C.+Do+not+compare+padding+data
// for more info.
typedef struct __attribute__((packed)) {
	int wstatus; ///< Recorded exit status of process. Obtained through "wstatus" argument from call to waitpid().

	/// @brief Expectation: if process terminated through call to exit(),
	/// what was the argument passed to exit().
	///
	/// It is expected that exp_exit_arg == WEXITSTATUS(wstatus).
	int exp_exit_arg;

	/// @brief Expectation: whether process terminated through call to exit().
	///
	/// It is expected that exp_exited == WIFEXITED(wstatus).
	bool exp_exited;
} event_sigchld_t;




typedef struct {
	event_type_t etype;       /**< Type of the event. */
	struct timespec tstamp;   /**< Time stamp of the event. */

	union {
		event_morse_receive_t morse;          /**< Morse code registered by cwdevice observer + Morse receiver on 'keying' pin of cwdevice. */
		test_reply_data_t reply; /**< Data received over socket by cwdaemon client. */
		event_sigchld_t sigchld;                     /**< Data collected by waitpid() in signal handler for SIGCHLD. */
	} u;
} event_t;




// Increased from 20 to 100 after long-running fuzzing test tried to add 21st
// event.
//
// On one hand I could/should make it a (doubly)linked list, but I don't want
// the hassle.
//
// On the other hand none of the current and foreseeable tests should insert
// more than few events.
//
// The fact that fuzzing test inserts 21+ events is only a result of the fact
// that fuzzing sub-tests don't clear events table after they complete. This
// will be addressed in the future.
#define EVENTS_MAX 100

typedef struct {
	event_t events[EVENTS_MAX];
	int events_cnt;               /**< Count of events in events array. Also indicates first non-occupied slot in events[]. */
	pthread_mutex_t mutex;
} events_t;




/**
   @brief Pretty-print events to tests' log output

   @reviewed_on{2024.04.18}

   @param[in] events Events structure to print
*/
void events_print(events_t const * events);




/**
   @brief Clear events structure

   This function can be used to erase old events from @p events structure.

   @reviewed_on{2024.04.18}

   @param[in/out] events Events structure to clear
*/
void events_clear(events_t * events);




/**
   @brief Sort events by timestamp

   This function sorts the events by their timestamp: from oldest to newest.

   Sometimes the events are inserted in non-chronological order. They need to
   be sorted before being evaluated.

   One example of non-chronological insert is with Morse receiver: Morse
   receiver remembers the time stamp of last received character, but the
   receiver is able to recognize that no more characters will come only after
   a longer time.

   An example sequence of events for Morse receiver is this:
   1. a character is received, timestamp of receiving it is saved in temporary
      variable.
   2. a space after the character is long enough to be recognized after some
      time as inter-word-space,
   3. some event unrelated to Morse code has occurred and is recorded into
      events array.
   4. nothing more is received after that time (after the inter-word-space),
      so receiver decides to save the timestamp from temporary variable into
      events array.

   So even though event from point 1 happened earlier, it is added to array
   of events after event from point 3.

   @reviewed_on{2024.04.18}

   @param[in/out] events Events structure in which the events should be sorted.

   @return 0
*/
int events_sort(events_t * events);




/// @brief Wrapper for easy inserting of "Morse received" event into events store
///
/// @exception assert(0) when there is no space available for the event in events
///
/// @param events Events store into which to insert the event
/// @param[in] buffer Buffer with received text
/// @param[in] last_character_receive_tstamp Timestamp at which the end of receiving of the last character has occurred
///
/// @return 0
int events_insert_morse_receive_event(events_t * events, const char * buffer, struct timespec * last_character_receive_tstamp);




/// @brief Wrapper for easy inserting of "reply received" event into events store
///
/// @exception assert(0) when there is no space available for the event in events
///
/// @param events Events store into which to insert the event
/// @param[in] received Received data
///
/// @return 0
int events_insert_reply_received_event(events_t * events, const test_reply_data_t * received);




/// @brief Wrapper for easy inserting of "SIGCHLD received" event into events store
///
/// @exception assert(0) when there is no space available for the event in events
///
/// @param events Events store into which to insert the event
/// @param[in] exit_info Information about child's exit status
///
/// @return 0
int events_insert_sigchld_event(events_t * events, const child_exit_info_t * exit_info);




/// @brief Wrapper for easy inserting of "EXIT Escape request was sent" event into events store
///
/// @exception assert(0) when there is no space available for the event in events
///
/// @param events Events store into which to insert the event
///
/// @return 0
int events_insert_exit_escape_request_event(events_t * events);




/**
   @brief Find event(s) of given type

   @p first_idx is updated by the function only if some event(s) of type @p
   type are found.

   @reviewed_on{2024.04.19}

   @param[in] events Events array in which to look for events of given type
   @param[in] type Type of event(s) to find in @p events
   @param[out] first_idx Index of first event of given type (if function returns value greater than zero)

   @return count of events of given type (may be zero)
   @return -1 on errors
*/
int events_find_by_type(events_t const * events, event_type_t type, int * first_idx);




/**
   @brief Get count of events with event type other than "none"

   Function stops counting after finding a first "none" event, or after reaching end of array.

   @reviewed_on{2024.04.19}

   @param[in] events Events array in which to count the events

   @return count of non-"none" events in @p events
*/
int events_get_count(const event_t events[EVENTS_MAX]);




#endif /* #ifndef CWDAEMON_TEST_LIB_EVENTS_H */

