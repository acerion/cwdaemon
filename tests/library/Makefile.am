# https://ftp.gnu.org/old-gnu/Manuals/automake-1.6.1/html_node/automake_5.html
#
# "The special prefix `check' indicates that the objects in question should
# not be built until the make check command is run. Those objects are not
# installed either."

.PHONY: check

# no header from this dir should be installed
check_HEADERS = client.h             \
                cw_rec_utils.h       \
                cwdevice_observer.h  \
                cwdevice_observer_serial.h  \
                misc.h               \
                process.h            \
                socket.h             \
                test_env.h

# convenience library
check_LIBRARIES = lib_tests.a

lib_tests_a_SOURCES = client.c             \
                      cw_rec_utils.c       \
                      cwdevice_observer.c  \
                      cwdevice_observer_serial.c  \
                      misc.c               \
                      process.c            \
                      socket.c             \
                      test_env.c

lib_tests_a_CFLAGS = $(LIBCW_CFLAGS) -pthread -DROOT_DIR=\"$(abs_top_builddir)\" -DLIBCW_LIBDIR=\"$(LIBCW_LIBDIR)\"

