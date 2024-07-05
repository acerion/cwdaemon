.PHONY: check

SUBDIRS = unit_tests

# These unit tests are for code that is used in cwdaemon.
TESTS  = unit_tests/daemon_utils
TESTS += unit_tests/daemon_options
TESTS += unit_tests/daemon_sleep




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

TESTS += functional_tests/unattended/option_cwdevice_tty_lines/test_program
TESTS += functional_tests/unattended/option_port/test_program
TESTS += functional_tests/unattended/reset_register_callback/test_program
TESTS += functional_tests/unattended/request_caret/test_program
TESTS += functional_tests/unattended/request_esc_exit/test_program
TESTS += functional_tests/unattended/request_esc_cwdevice/test_program
TESTS += functional_tests/unattended/request_esc_reply/test_program
TESTS += functional_tests/unattended/request_plain/test_program

# We build fuzzing tests, but we don't run them on "make check".
# TESTS += fuzzing/simple/test_program

# We build supervised tests, but we don't run them on "make check".
# TESTS += functional_tests/supervised/feature_multiple_requests/test_program
# TESTS += functional_tests/supervised/request_esc_port/test_program
# TESTS += functional_tests/supervised/request_esc_sound_system/test_program

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

