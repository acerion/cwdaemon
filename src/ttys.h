#ifndef H_CWDAEMON_TTYS
#define H_CWDAEMON_TTYS




// Forward declaration.
struct cwdev_s;




typedef struct driveroptions {
	int key; // Pin/line used for keying. TIOCM_DTR by default.
	int ptt; // Pin/line used for PTT.    TIOCM_RTS by default.
} driveroptions;




/// @brief Initialize cwdaemon's global tty cwdevice structure
///
/// @param[in/out] dev tty cwdevice structure to initialize
///
/// @return 0 on success
/// @return -1 on failure
int tty_init_global_cwdevice(struct cwdev_s * dev);




#endif /* #ifndef H_CWDAEMON_TTYS */

