#ifndef H_CW_EASY_RECEIVER
#define H_CW_EASY_RECEIVER




#include <stdbool.h>
#include <sys/time.h>

#include <libcw2.h>




#if defined(__cplusplus)
extern "C"
{
#endif




#if 0
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
#endif




struct cw_easy_rec_t;
typedef struct cw_easy_rec_t cw_easy_rec_t;




cw_easy_rec_t * cw_easy_rec_new(void);
void cw_easy_rec_delete(cw_easy_rec_t ** easy_rec);


cw_ret_t cw_easy_rec_poll_with_callback(cw_easy_rec_t * easy_rec, int (* callback)(const cw_rec_data_t *));
cw_ret_t cw_easy_rec_poll_data(cw_easy_rec_t * easy_rec, cw_rec_data_t * data);


int cw_easy_rec_get_libcw_errno(const cw_easy_rec_t * easy_rec);
void cw_easy_rec_clear_libcw_errno(cw_easy_rec_t * easy_rec);
bool cw_easy_rec_is_pending_inter_word_space(const cw_easy_rec_t * easy_rec);
void cw_easy_rec_clear_buffer_and_state(cw_easy_rec_t * easy_rec);




/// @brief libcw receiver's callback to be called on change of straight key's state
///
/// This is a callback for objects of type cw_easy_rec_t. It should be
/// called on each change of state (open/closed, up/down) of straight key.
///
/// In the context of cwdaemon, the straight key is the "keying" pin on
/// cwdevice.
///
/// @param[in/out] easy_receiver cw_easy_rec_t receiver structure
/// @param[in] key_state Whether straight key is down or up
///
/// @return 0
int cw_easy_rec_handle_keying_event(void * easy_receiver, int key_state);




cw_ret_t cw_easy_rec_set_speed(cw_easy_rec_t * easy_rec, int speed);
cw_ret_t cw_easy_rec_get_speed(cw_easy_rec_t * easy_rec, float * speed);
cw_ret_t cw_easy_rec_set_tolerance(cw_easy_rec_t * easy_rec, int tolerance);
cw_ret_t cw_easy_rec_get_tolerance(const cw_easy_rec_t * easy_rec, int * tolerance);
// void cw_easy_rec_register_receive_callback(cw_easy_rec_t * easy_rec, cw_easy_rec_receive_callback_t cb, void * data);
cw_ret_t cw_easy_rec_init_tracked_key_state(cw_easy_rec_t * rec, int key_state);




#if defined(__cplusplus)
}
#endif




#endif // #ifndef H_CW_EASY_RECEIVER

