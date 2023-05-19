.PHONY: check


# Programs to be built when "make check" target is built.
check_PROGRAMS=utils

# Don't forget to use a relative directory path even for files in current
# dir.
utils_SOURCES  = ../../src/utils.c ./utils.c
utils_CPPFLAGS = -I$(top_srcdir)
