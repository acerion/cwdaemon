#ifndef H_CW_EASY_REC
#define H_CW_EASY_REC




#include <libcw2.h>




#if defined(__cplusplus)
extern "C"
{
#endif




struct cw_easy_rec_t;
typedef struct cw_easy_rec_t cw_easy_rec_t;




#if 0
/**
   @brief Callback executed on successful receiving of single data point (single character) by receiver

   @param[in] callback_data Pointer to some object in an application that is using the receiver
   @param[in] erd Data with information about what was received by the receiver
*/
typedef void (*cw_easy_rec_receive_callback_t)(void * callback_data, cw_rec_data_t * data);
#endif




/// @brief Constructor of easy receiver object
///
/// Objects created with the constructor should be deleted with cw_easy_rec_delete().
///
/// @return Easy receiver on success
/// @return NULL on failure
cw_easy_rec_t * cw_easy_rec_new(void);




/// @brief Destructor of easy receiver object
///
/// Resources associated with the easy receiver are deallocated and the @p
/// easy_rec pointer is set to NULL.
//
/// Call the destructor on objects returned by cw_easy_rec_new() constructor.
///
/// @param[in/out] easy_rec Pointer to easy receiver that should be deleted
void cw_easy_rec_delete(cw_easy_rec_t ** easy_rec);




#if 0
/**
   \brief Start easy receiver

   Let the receiver start receiving.

   Notice that without calling
   cw_gen_register_value_tracking_callback_internal() and
   cw_easy_rec_register_receive_callback() the receiver won't be doing anything
   useful.

   @reviewedon 2023.08.12

   @param[in/out] easy_rec Easy receiver that should start receiving
*/
void cw_easy_rec_start(cw_easy_rec_t * easy_rec);




/**
   \brief Stop easy receiver

   Stop the process of receiving by given receiver.

   @reviewedon 2023.08.12

   @param[in/out] easy_rec Easy receiver to stop
*/
void cw_easy_rec_stop(cw_easy_rec_t * easy_rec);
#endif




/// @brief Poll the easy receiver for data. Return results through @p data on successful poll.
///
/// Poll given easy receiver for character or inter-word-space.
///
/// The main function to be used by client code when the client code is
/// getting data from receiver by periodically polling it.
///
/// @p data variable is allocated and owned by caller.
///
/// @param[in] easy_rec Easy receiver to poll
/// @param[out] data Data polled from receiver
///
/// @return CW_SUCCESS on successful polling some (data was available in receiver and was returned through @p data
/// @return CW_FAILURE on unsuccessful polling (no new data was available in receiver)
cw_ret_t cw_easy_rec_poll(cw_easy_rec_t * easy_rec, cw_rec_data_t * data);




/// @brief Poll the easy receiver for data. Call callback on successful poll.
///
/// @param[in] easy_rec Easy receiver to poll
/// @param[out] data Data polled from receiver
///
/// @return CW_SUCCESS on successful polling (some data was available in receiver and @p callback was called)
/// @return CW_FAILURE on unsuccessful polling (no new data was available in receiver)
cw_ret_t cw_easy_rec_poll_with_callback(cw_easy_rec_t * easy_rec, int (* callback)(const cw_rec_data_t *));




cw_ret_t cw_easy_rec_get_libcw_errno(const cw_easy_rec_t * easy_rec, int * err);
void cw_easy_rec_clear_libcw_errno(cw_easy_rec_t * easy_rec);
void cw_easy_rec_clear_buffer_and_state(cw_easy_rec_t * easy_rec);




/**
   @brief Wrapper around cw_rec_set_speed() for easy receiver

   Right now there is no way to set easy receiver in adaptive mode.

   @reviewedon 2023.08.12

   @param[in] easy_rec Easy receiver for which to set the speed
   @param[in] speed New value of speed

   @return CW_SUCCESS on success
   @return CW_FAILURE if function failed to set speed in receiver
*/
cw_ret_t cw_easy_rec_set_speed(cw_easy_rec_t * easy_rec, int speed);




/**
   @brief Wrapper around cw_rec_get_speed() for easy receiver

   @reviewedon 2023.09.29

   @param[in] easy_rec Easy receiver for which to get the speed
   @param[in] speed Value of speed of @p receiver

   @return CW_SUCCESS on success
   @return CW_FAILURE if function failed to get speed in receiver
*/
cw_ret_t cw_easy_rec_get_speed(cw_easy_rec_t * easy_rec, float * speed);




/**
   @brief Wrapper around cw_rec_set_tolerance() for easy receiver

   @reviewedon 2023.08.12

   @param[in] easy_rec Easy receiver for which to set the tolerance
   @param[in] tolerance New value oftolerance

   @return CW_SUCCESS on success
   @return CW_FAILURE if function failed to set tolerance in receiver
*/
cw_ret_t cw_easy_rec_set_tolerance(cw_easy_rec_t * rec, int tolerance);




/**
   @brief Wrapper around cw_rec_get_tolerance() for easy receiver

   @reviewed_on{2023.10.27}

   @param[in] easy_rec Easy receiver from which to get the tolerance
   @param[out] tolerance tolerance of the receiver

   @return CW_SUCCESS if function returns a tolerance (through @p tolerance) successfully
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_easy_rec_get_tolerance(const cw_easy_rec_t * easy_rec, int * tolerance);




#if 0
/**
   \brief Register a callback that will be called whenever successful receive occurs

   'successful receive' means that either a character or an
   inter-word-space has been received.

   The callback will be called on each successful receive. A variable of type
   cw_rec_data_t that contains details of current successful receive
   event will be passed to the callback.

   @reviewedon 2023.08.12

   @param[in/out] easy_rec Easy receiver with which to register a callback
   @param[in] callback Callback to be registered - function that will be called on each successful receive
   @param[in] data Pointer to client-side variable that will be passed to @p callback
*/
void cw_easy_rec_register_receive_callback(cw_easy_rec_t * easy_rec, cw_easy_rec_receive_callback_t callback, void * data);
#endif




/// @brief Handler for keying event
///
/// This function should be called by client code on each keying event.
///
/// "Keying event" means each change of your straight key, or your Morse code
/// generator, or your detector of Morse code in your Software Defined Radio.
///
/// Each time you detect start/end of Dot/Dash, you should call this function
/// to let libcw's easy receiver that something should be received and
/// interpreted by the receiver, possibly resulting in a new character or
/// inter-word-space.
///
/// The function has "keying" in its name, and the second argument is
/// "key_state" because we can imagine that the handled events come from some
/// (physical or virtual) Morse key.
///
/// @param[in/out] easy_receiver Easy receiver
/// @param[in] key_state CW_KEY_STATE_OPEN or CW_KEY_STATE_CLOSED
///
/// @return 0 on successful handling of event by easy receiver
/// @return -1 otherwise
int cw_easy_rec_handle_keying_event(void * easy_receiver, int key_state);




cw_ret_t cw_easy_rec_init_tracked_key_state(cw_easy_rec_t * rec, int key_state);




#if defined(__cplusplus)
}
#endif




#endif // #ifndef H_CW_EASY_REC

