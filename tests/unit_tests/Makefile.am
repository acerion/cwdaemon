.PHONY: check


# Programs to be built when "make check" target is built.
check_PROGRAMS  = daemon_options daemon_utils daemon_sleep
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
	make gcov2 target=daemon_utils
	make gcov2 target=daemon_options
	make gcov2 target=daemon_sleep


gcov2:
	@echo "[II] Coverage: removing old artifacts before building unit test [$(target)]"
	make clean
	make clean -C $(abs_top_builddir)/src/
	make gcov-clean
	make gcov-clean -C $(abs_top_builddir)/src/
	rm -rf $(OUTPUT_DIR)/unit_tests/$(target)

	@echo "[II] Coverage: building unit test [$(target)]"
	make $(target)

	@echo "[II] Coverage: running unit test [$(target)]"
	./$(target)

	@echo "[II] Coverage: building report for unit test [$(target)]"
	mkdir -p $(OUTPUT_DIR)/unit_tests/$(target)
	lcov -directory $(abs_top_builddir)/tests/unit_tests --capture --output-file $(OUTPUT_DIR)/unit_tests/$(target)/traceinfo.info
	genhtml -t $(target) -o $(OUTPUT_DIR)/unit_tests/$(target) $(OUTPUT_DIR)/unit_tests/$(target)/traceinfo.info


gcov-clean:
	rm -f *.gcda *.gcno

endif




# Below are unit tests for code used in daemon.

daemon_utils_SOURCES  = $(top_srcdir)/src/utils.c ./daemon_utils.c
daemon_utils_CPPFLAGS = -I$(top_srcdir) $(gcov_C_FLAGS)
daemon_utils_LDFLAGS  = $(gcov_LD_FLAGS)


daemon_options_SOURCES  = $(top_srcdir)/src/options.c $(top_srcdir)/src/log.c $(top_srcdir)/src/utils.c ./daemon_options.c ./daemon_stubs.c
daemon_options_CPPFLAGS = -I$(top_srcdir) $(gcov_C_FLAGS)
daemon_options_LDFLAGS  = $(gcov_LD_FLAGS)


daemon_sleep_SOURCES  = $(top_srcdir)/src/sleep.c ./daemon_sleep.c
daemon_sleep_CPPFLAGS = -I$(top_srcdir) $(gcov_C_FLAGS)
daemon_sleep_LDFLAGS  = $(gcov_LD_FLAGS)




# Below are unit tests for code used in functional tests.

tests_string_utils_SOURCES  = $(top_srcdir)/tests/library/string_utils.c ./tests_string_utils.c
tests_string_utils_CPPFLAGS = -I$(top_srcdir)

tests_time_utils_SOURCES  = $(top_srcdir)/tests/library/time_utils.c ./tests_time_utils.c
tests_time_utils_CPPFLAGS = -I$(top_srcdir)

tests_random_SOURCES  = $(top_srcdir)/tests/library/random.c $(top_srcdir)/tests/library/random.h ./tests_random.c
tests_random_CPPFLAGS = -I$(top_srcdir)

tests_morse_receiver_SOURCES = $(top_srcdir)/tests/library/morse_receiver_utils.c ./tests_morse_receiver.c
tests_morse_receiver_CPPFLAGS = -I$(top_srcdir)

tests_events_SOURCES = $(top_srcdir)/tests/library/events.c        \
                       $(top_srcdir)/tests/library/random.c        \
                       $(top_srcdir)/tests/library/string_utils.c  \
                       $(top_srcdir)/tests/library/time_utils.c    \
                       ./tests_events.c
tests_events_CPPFLAGS = -I$(top_srcdir) $(LIBCW_CFLAGS)



