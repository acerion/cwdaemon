.PHONY: check


# Programs to be built when "make check" target is built.
check_PROGRAMS = random utils

# Don't forget to use a relative directory path even for files in current
# dir.

random_SOURCES  = $(top_builddir)/src/lib/random.c $(top_builddir)/src/lib/random.h ./random.c
random_CPPFLAGS = -I$(top_srcdir)

utils_SOURCES  = ../../src/utils.c ./utils.c
utils_CPPFLAGS = -I$(top_srcdir)

