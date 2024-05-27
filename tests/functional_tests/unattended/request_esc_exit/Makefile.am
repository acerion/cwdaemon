# Program(s) to be built in current dir when "check" target is invoked.
check_PROGRAMS = test_program

# Source code files used to build "test_program".
test_program_SOURCES = main.c

# Target-specific preprocessor flags (#defs and include dirs).
test_program_CPPFLAGS = ${AM_CFLAGS} ${LIBCW_CFLAGS}

# Target-specific linker flags (objects to link).
test_program_LDADD = $(top_builddir)/tests/library/lib_tests.a ${LIBCW_LIBS}

# Target-specific compiler flags.
test_program_CFLAGS = -I$(top_srcdir) -pthread


