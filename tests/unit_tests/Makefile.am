.PHONY: check


# Programs to be built when "make check" target is built.
check_PROGRAMS  = daemon_options daemon_utils
if FUNCTIONAL_TESTS
check_PROGRAMS += tests_random tests_time_utils
endif




# Don't forget to use a relative directory path even for files in current
# dir.




# Below are unit tests for code used in daemon.

daemon_utils_SOURCES  = ../../src/utils.c ./daemon_utils.c
daemon_utils_CPPFLAGS = -I$(top_srcdir)

daemon_options_SOURCES  = ../../src/options.c ../../src/log.c ../../src/utils.c ./daemon_options.c
daemon_options_CPPFLAGS = -I$(top_srcdir)




# Below are unit tests for code used in functional tests.

tests_time_utils_SOURCES  = ../library/time_utils.c ./tests_time_utils.c
tests_time_utils_CPPFLAGS = -I$(top_srcdir)

tests_random_SOURCES  = ../library/random.c ../library/random.h ./tests_random.c
tests_random_CPPFLAGS = -I$(top_srcdir)


