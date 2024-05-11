#ifndef H_CWDAEMON_TTYS
#define H_CWDAEMON_TTYS




// Forward declaration.
struct cwdev_s;




typedef struct tty_driver_options {
	unsigned int key; // Pin/line used for keying. TIOCM_DTR by default. "unsigned" because TIOCM_* is defined as hex value.
	unsigned int ptt; // Pin/line used for PTT.    TIOCM_RTS by default. "unsigned" because TIOCM_* is defined as hex value.
} tty_driver_options;




/// @brief Initialize "struct cwdev_s" variable for tty cwdevice
///
/// @param[in/out] dev tty cwdevice structure to initialize
///
/// @return 0 on success
/// @return -1 on failure
int tty_init_cwdevice(struct cwdev_s * dev);




/// @brief Try opening a serial console cwdevice with given device name
///
/// Check to see whether @p fname is a tty type character device capable of
/// TIOCM*. This should be platform independent.
///
/// @return -1 if the device isn't a suitable tty device
/// @return a file descriptor if the device is a suitable tty device
int tty_probe_cwdevice(const char * fname);




#endif /* #ifndef H_CWDAEMON_TTYS */

