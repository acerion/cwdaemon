# no header from this dir should be installed
noinst_HEADERS = cw_rec_utils.h       \
                 key_source.h         \
                 key_source_serial.h  \
                 misc.h               \
                 process.h            \
                 socket.h

# convenience library
noinst_LIBRARIES = lib_cwdaemon_tests.a

lib_cwdaemon_tests_a_SOURCES = cw_rec_utils.c       \
                               key_source.c         \
                               key_source_serial.c  \
                               misc.c               \
                               process.c            \
                               socket.c

lib_cwdaemon_tests_a_CFLAGS = $(LIBCW_CFLAGS) -pthread -DROOT_DIR=\"$(abs_top_builddir)\" -DLIBCW_LIBDIR=\"$(LIBCW_LIBDIR)\"
