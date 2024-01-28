.PHONY: check


# Programs to be built when "make check" target is built.
check_PROGRAMS  = daemon_options daemon_utils
if FUNCTIONAL_TESTS
check_PROGRAMS += tests_random tests_time_utils tests_morse_receiver
endif




# Don't forget to use a relative directory path even for files in current
# dir.




if ENABLE_GCOV
gcov_CFLAGS = --coverage

gcov:
	rm -rf coverage
	mkdir coverage
	lcov --compat-libtool -directory ../../src/ --capture --output-file coverage/utests.info
	genhtml -o coverage/ coverage/utests.info

gcov-clean:
	rm -rf coverage
	rm -f *.gcda *.gcno
	rm -f ../../src/*.gcda ../../src/*.gcno
endif




# Below are unit tests for code used in daemon.

daemon_utils_SOURCES  = ../../src/utils.c ./daemon_utils.c
daemon_utils_CPPFLAGS = -I$(top_srcdir) $(gcov_CFLAGS)
if ENABLE_GCOV
daemon_utils_LDFLAGS  = --coverage
endif


daemon_options_SOURCES  = ../../src/options.c ../../src/log.c ../../src/utils.c ./daemon_options.c
daemon_options_CPPFLAGS = -I$(top_srcdir) $(gcov_CFLAGS)
if ENABLE_GCOV
daemon_options_LDFLAGS  = --coverage
endif




# Below are unit tests for code used in functional tests.

tests_time_utils_SOURCES  = ../library/time_utils.c ./tests_time_utils.c
tests_time_utils_CPPFLAGS = -I$(top_srcdir)

tests_random_SOURCES  = ../library/random.c ../library/random.h ./tests_random.c
tests_random_CPPFLAGS = -I$(top_srcdir)

tests_morse_receiver_SOURCES = ../library/morse_receiver_utils.c ./tests_morse_receiver.c
tests_morse_receiver_CPPFLAGS = -I$(top_srcdir)



