# program(s) to be built in current dir
check_PROGRAMS = test

# source code files used to build cwdaemon program
test_SOURCES = main.c
# target-specific preprocessor flags (#defs and include dirs)
test_CPPFLAGS = ${AM_CFLAGS} ${LIBCW_CFLAGS}
# target-specific linker flags (objects to link)
test_LDADD = ${LIBCW_LIBS} $(top_builddir)/test/library/lib_cwdaemon_test.a

test_CFLAGS = -std=c99 -pthread

test_LDFLAGS = -Wl,-rpath=${prefix}/lib

