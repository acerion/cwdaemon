.PHONY: check daemon_utils.dummy daemon_options.dummy


# Programs to be built when "make check" target is built.
check_PROGRAMS  = daemon_options daemon_utils
if FUNCTIONAL_TESTS
check_PROGRAMS += tests_random \
                  tests_string_utils \
                  tests_time_utils \
                  tests_morse_receiver \
                  tests_events
endif




# Don't forget to use a relative directory path even for files in current
# dir.




if ENABLE_GCOV

gcov_C_FLAGS  = --coverage
gcov_LD_FLAGS = --coverage


# Target for building coverage report for unit tests. But only for those unit
# tests that test code of main program (of cwdaemon). There is no report for
# unit tests executed on testing code itself.
gcov:
	make daemon_utils.gcov
	make daemon_options.gcov


# POSIX-compliant "suffix rules" are SO MUCH FUN!
#
# This rule is doing following:
# 1. clean unit test code
# 2. rebuild unit test code
# 3. run unit test
# 4. generate coverage report with lcov+genhtml
SUFFIXES=.gcov .dummy
.dummy.gcov:
	make clean
	make clean -C $(abs_top_builddir)/src/
	make gcov-clean
	make gcov-clean -C $(abs_top_builddir)/src/
	rm -rf $(OUTPUT_DIR)/unit_tests/$*
	make $*
	./$*
	mkdir -p $(OUTPUT_DIR)/unit_tests/$*
	lcov -directory $(abs_top_builddir)/src/ --capture --output-file $(OUTPUT_DIR)/unit_tests/$*/traceinfo.info
	genhtml -t $* -o $(OUTPUT_DIR)/unit_tests/$* $(OUTPUT_DIR)/unit_tests/$*/traceinfo.info


gcov-clean:
	rm -f *.gcda *.gcno

endif




# Below are unit tests for code used in daemon.

daemon_utils_SOURCES  = ../../src/utils.c ./daemon_utils.c
daemon_utils_CPPFLAGS = -I$(top_srcdir) $(gcov_C_FLAGS)
daemon_utils_LDFLAGS  = $(gcov_LD_FLAGS)


daemon_options_SOURCES  = ../../src/options.c ../../src/log.c ../../src/utils.c ./daemon_options.c
daemon_options_CPPFLAGS = -I$(top_srcdir) $(gcov_C_FLAGS)
daemon_options_LDFLAGS  = $(gcov_LD_FLAGS)




# Below are unit tests for code used in functional tests.

tests_string_utils_SOURCES  = $(abs_top_srcdir)/tests/library/string_utils.c ./tests_string_utils.c
tests_string_utils_CPPFLAGS = -I$(abs_top_srcdir)

tests_time_utils_SOURCES  = ../library/time_utils.c ./tests_time_utils.c
tests_time_utils_CPPFLAGS = -I$(top_srcdir)

tests_random_SOURCES  = ../library/random.c ../library/random.h ./tests_random.c
tests_random_CPPFLAGS = -I$(top_srcdir)

tests_morse_receiver_SOURCES = ../library/morse_receiver_utils.c ./tests_morse_receiver.c
tests_morse_receiver_CPPFLAGS = -I$(top_srcdir)

tests_events_SOURCES = ../library/events.c        \
                       ../library/random.c        \
                       ../library/string_utils.c  \
                       ../library/time_utils.c    \
                       ./tests_events.c
tests_events_CPPFLAGS = -I$(top_srcdir) $(LIBCW_CFLAGS)



