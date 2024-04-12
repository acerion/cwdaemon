#ifndef H_CWDAEMON_TTYS
#define H_CWDAEMON_TTYS




// Forward declaration.
struct cwdev_s;




typedef struct tty_driver_options {
	int key; // Pin/line used for keying. TIOCM_DTR by default.
	int ptt; // Pin/line used for PTT.    TIOCM_RTS by default.
} tty_driver_options;




/// @brief Initialize cwdaemon's global tty cwdevice structure
///
/// @param[in/out] dev tty cwdevice structure to initialize
///
/// @return 0 on success
/// @return -1 on failure
int tty_init_global_cwdevice(struct cwdev_s * dev);




/// @brief Get file descriptor for serial port
///
/// Check to see whether @p fname is a tty type character device capable of
/// TIOCM*. This should be platform independent.
///
/// @return -1 if the device isn't a suitable tty device
/// @return a file descriptor if the device is a suitable tty device
int tty_get_file_descriptor(const char * fname);




#endif /* #ifndef H_CWDAEMON_TTYS */

