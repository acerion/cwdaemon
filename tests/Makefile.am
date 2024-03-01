.PHONY: check

SUBDIRS = unit_tests

# These unit tests are for code that is used in cwdaemon.
TESTS  = unit_tests/daemon_utils
TESTS += unit_tests/daemon_options




if FUNCTIONAL_TESTS

SUBDIRS += library
SUBDIRS += functional_tests
SUBDIRS += fuzzing

# These unit tests are for code that is used only in functional tests.
TESTS += unit_tests/tests_random
TESTS += unit_tests/tests_string_utils
TESTS += unit_tests/tests_time_utils
TESTS += unit_tests/tests_morse_receiver
TESTS += unit_tests/tests_events

# Notice that tests from "functional_tests/manual" are not included here. The
# manual tests should be built on "make check", but not executed on "make
# check".
TESTS += functional_tests/automatic/option_cwdevice_tty_lines/test_program
TESTS += functional_tests/automatic/option_port/test_program
TESTS += functional_tests/automatic/reset_register_callback/test_program
TESTS += functional_tests/automatic/request_caret/test_program
TESTS += functional_tests/automatic/request_esc_5/test_program
TESTS += functional_tests/automatic/request_esc_h/test_program

# We don't run fuzzing tests on "make check".
# TESTS += fuzzing/simple/test_program

endif




if ENABLE_GCOV

gcov:
	@echo "Coverage: creating dir for reports"
	mkdir -p $(abs_top_builddir)/tests/coverage/

	@echo "Coverage: building unit tests, running them and generating coverage reports"
	make gcov -C unit_tests OUTPUT_DIR=$(abs_top_builddir)/tests/coverage/

if FUNCTIONAL_TESTS
	@echo "Coverage: building server to be used by functional tests"
	make gcov-clean -C $(abs_top_builddir)/src/
	make -C $(abs_top_builddir)/src/

	@echo "Coverage: building utility library for functional tests"
	make check -C library

	@echo "Coverage: building functional tests, running them and generating coverage reports"
	make gcov -C functional_tests OUTPUT_DIR=$(abs_top_builddir)/tests/coverage/
endif


gcov-clean:
	rm -rf $(abs_top_builddir)/tests/coverage/
	make gcov-clean -C unit_tests

endif

