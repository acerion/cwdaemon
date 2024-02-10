# https://ftp.gnu.org/old-gnu/Manuals/automake-1.6.1/html_node/automake_5.html
#
# "The special prefix `check' indicates that the objects in question should
# not be built until the make check command is run. Those objects are not
# installed either."

.PHONY: check

# no header from this dir should be installed
check_HEADERS = client.h             \
                cw_easy_receiver.h   \
                cwdevice_observer.h  \
                cwdevice_observer_serial.h  \
                events.h             \
                log.h                \
                misc.h               \
                morse_receiver.h     \
                morse_receiver_utils.h  \
                random.h             \
                server.h             \
                sleep.h              \
                socket.h             \
                test_env.h           \
                time_utils.h         \
                thread.h

# convenience library
check_LIBRARIES = lib_tests.a

lib_tests_a_SOURCES = client.c             \
                      cw_easy_receiver.c   \
                      cwdevice_observer.c  \
                      cwdevice_observer_serial.c  \
                      events.c             \
                      misc.c               \
                      morse_receiver.c     \
                      morse_receiver_utils.c \
                      random.c             \
                      server.c            \
                      sleep.c              \
                      socket.c             \
                      test_env.c           \
                      time_utils.c         \
                      thread.c

lib_tests_a_CFLAGS = $(LIBCW_CFLAGS) -pthread -DLIBCW_LIBDIR=\"$(LIBCW_LIBDIR)\"

