.PHONY: check


# Programs to be built when "make check" target is built.
check_PROGRAMS = random utils time_utils test_options

# Don't forget to use a relative directory path even for files in current
# dir.

random_SOURCES  = ../library/random.c ../library/random.h ./random.c
random_CPPFLAGS = -I$(top_srcdir)

utils_SOURCES  = ../../src/utils.c ./utils.c
utils_CPPFLAGS = -I$(top_srcdir)

time_utils_SOURCES  = ../library/time_utils.c ./time_utils.c
time_utils_CPPFLAGS = -I$(top_srcdir)

test_options_SOURCES  =  ./options.c ../../src/options.c ../../src/log.c ../../src/utils.c
test_options_CPPFLAGS = -I$(top_srcdir)


