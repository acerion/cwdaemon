# program(s) to be built in current dir
check_PROGRAMS = test

# source code files used to build cwdaemon program
test_SOURCES = main.c
# target-specific preprocessor flags (#defs and include dirs)
test_CPPFLAGS = ${AM_CFLAGS} ${LIBCW_CFLAGS}
# target-specific linker flags (objects to link)
test_LDADD = $(top_builddir)/test/library/lib_cwdaemon_test.a ${LIBCW_LIBS}

test_CFLAGS = -std=c99 -pthread

test_LDFLAGS = -Wl,-rpath=${LIBCW_LIBDIR}

