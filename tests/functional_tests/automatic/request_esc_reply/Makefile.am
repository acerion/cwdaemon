# program(s) to be built in current dir
check_PROGRAMS = test_program

# source code files used to build cwdaemon program
test_program_SOURCES  = main.c
test_program_SOURCES += basic.c basic.h
test_program_SOURCES += request_size.c request_size.h
test_program_SOURCES += shared.c shared.h

# target-specific preprocessor flags (#defs and include dirs)
test_program_CPPFLAGS = ${AM_CFLAGS} ${LIBCW_CFLAGS}

# target-specific linker flags (objects to link)
test_program_LDADD = $(top_builddir)/tests/library/lib_tests.a ${LIBCW_LIBS}

test_program_CFLAGS = -std=c99 -pthread


