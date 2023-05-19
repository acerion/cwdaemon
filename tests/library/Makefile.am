# no header from this dir should be installed
noinst_HEADERS = cw_rec_utils.h       \
                 key_source.h         \
                 key_source_serial.h  \
                 misc.h               \
                 process.h            \
                 socket.h             \
                 test_env.h

# convenience library
noinst_LIBRARIES = lib_tests.a

lib_tests_a_SOURCES = cw_rec_utils.c       \
                      key_source.c         \
                      key_source_serial.c  \
                      misc.c               \
                      process.c            \
                      socket.c             \
                      test_env.c

lib_tests_a_CFLAGS = $(LIBCW_CFLAGS) -pthread -DROOT_DIR=\"$(abs_top_builddir)\" -DLIBCW_LIBDIR=\"$(LIBCW_LIBDIR)\"

