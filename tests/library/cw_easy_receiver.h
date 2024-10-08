#ifndef H_CW_EASY_RECEIVER
#define H_CW_EASY_RECEIVER




#include <stdbool.h>
#include <sys/time.h>




#if defined(__cplusplus)
extern "C"
{
#endif




typedef struct cw_easy_rec_t {
	/* Timer for measuring length of dots and dashes.

	   Initial value of the timestamp is created by xcwcp's receiver on
	   first "paddle down" event in a character. The timestamp is then
	   updated by libcw on specific time intervals. The intervals are a
	   function of keyboard key presses or mouse button presses recorded
	   by xcwcp. */
	struct timeval main_timer;

	/* Safety flag to ensure that we keep the library in sync with keyer
	   events. Without, there's a chance that of a on-off event, one half
	   will go to one application instance, and the other to another
	   instance. */
	volatile bool tracked_key_is_down;

	/* Flag indicating if receive polling has received a character, and
	   may need to augment it with a word space on a later poll. */
	volatile bool is_pending_iws;

	/* Flag indicating possible receive errno detected in signal handler
	   context and needing to be passed to the foreground. */
	volatile int libcw_receive_errno;

	/* State of left and right paddle of iambic keyer. The
	   flags are common for keying with keyboard keys and
	   with mouse buttons.

	   A timestamp for libcw needs to be generated only in
	   situations when one of the paddles comes down and
	   the other is up. This is why we observe state of
	   both paddles separately. */
	bool is_left_down;
	bool is_right_down;

	/* Whether to get a representation or a character from receiver's
	   internals with libcw low-level API. */
	bool get_representation;

	void * rec_tester;
} cw_easy_rec_t;




/* TODO: move this type to libcw_rec.h and use it to pass arguments to
   functions such as cw_rec_poll_representation_ics_internal(). */
#define REPRESENTATION_SIZE 20 /* TODO 2024.03.02: move the define to libcw.h? */
typedef struct cw_rec_data_t {
	char character;
	char representation[REPRESENTATION_SIZE];
	int errno_val;
	bool is_iws;             /* Is receiver in 'found inter-word-space' state? */
	bool is_error;
} cw_rec_data_t;




/* *** For legacy libcw API. *** */




cw_easy_rec_t * cw_easy_receiver_new(void);
void cw_easy_receiver_delete(cw_easy_rec_t ** easy_rec);
void cw_easy_receiver_start(cw_easy_rec_t * easy_rec);


bool cw_easy_receiver_poll(cw_easy_rec_t * easy_rec, int (* callback)(const cw_rec_data_t *));
bool cw_easy_receiver_poll_data(cw_easy_rec_t * easy_rec, cw_rec_data_t * erd);
bool cw_easy_receiver_poll_character(cw_easy_rec_t * easy_rec, cw_rec_data_t * erd);
bool cw_easy_receiver_poll_space(cw_easy_rec_t * easy_rec, cw_rec_data_t * erd);

int cw_easy_receiver_get_libcw_errno(const cw_easy_rec_t * easy_rec);
void cw_easy_receiver_clear_libcw_errno(cw_easy_rec_t * easy_rec);
bool cw_easy_receiver_is_pending_inter_word_space(const cw_easy_rec_t * easy_rec);
void cw_easy_receiver_clear(cw_easy_rec_t * easy_rec);




/**
   \brief Handle straight key event

   \param is_down
*/
void cw_easy_receiver_sk_event(cw_easy_rec_t * easy_rec, bool is_down);

/**
   \brief Handle event on left paddle of iambic keyer

   \param is_down
   \param is_reverse_paddles
*/
void cw_easy_receiver_ik_left_event(cw_easy_rec_t * easy_rec, bool is_down, bool is_reverse_paddles);

/**
   \brief Handle event on right paddle of iambic keyer

   \param is_down
   \param is_reverse_paddles
*/
void cw_easy_receiver_ik_right_event(cw_easy_rec_t * easy_rec, bool is_down, bool is_reverse_paddles);




/// @brief libcw receiver's callback to be called on change of straight key's state
///
/// This is a callback for objects of type cw_easy_rec_t. It should be
/// called on each change of state (open/closed, up/down) of straight key.
///
/// In the context of cwdaemon, the straight key is the "keying" pin on
/// cwdevice.
///
/// This function is similar to cw_easy_receiver_handle_libcw_keying_event(),
/// but has a prototype suitable for passing as callback to
/// cw_register_keying_callback().
///
/// @reviewed_on{2024.04.22}
///
/// @param[in/out] easy_receiver cw_easy_rec_t receiver structure
/// @param[in] key_state CW_KEY_STATE_OPEN or CW_KEY_STATE_CLOSED
void cw_easy_receiver_handle_libcw_keying_event_void(void * easy_receiver, int key_state);




/// @brief libcw receiver's callback to be called on change of straight key's state
///
/// This is a callback for objects of type cw_easy_rec_t. It should be
/// called on each change of state (open/closed, up/down) of straight key.
///
/// In the context of cwdaemon, the straight key is the "keying" pin on
/// cwdevice.
///
/// @param[in/out] easy_receiver cw_easy_rec_t receiver structure
/// @param[in] key_is_down Whether straight key is down or up
///
/// @return 0
int cw_easy_receiver_handle_libcw_keying_event(void * easy_receiver, bool key_is_down);




/// @brief Inform an easy receiver that a key pin has a new state (up or down)
///
/// A simple wrapper that seems to be convenient.
///
/// @reviewed_on{2024.04.16}
///
/// @param[in] arg_easy_rec cw_easy_rec_t variable
/// @param[in] key_is_down current state of keying pin
///
/// @return 0
int cw_easy_receiver_on_key_state_change(void * arg_easy_rec, bool key_is_down);




#if defined(__cplusplus)
}
#endif




#endif // #ifndef H_CW_EASY_RECEIVER

