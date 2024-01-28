.PHONY: check

SUBDIRS = unit_tests

# These unit tests are for code that is used in cwdaemon.
TESTS  = unit_tests/daemon_utils
TESTS += unit_tests/daemon_options




if FUNCTIONAL_TESTS

SUBDIRS += library
SUBDIRS += functional_tests

# These unit tests are for code that is used only in functional tests.
TESTS += unit_tests/tests_random
TESTS += unit_tests/tests_time_utils
TESTS += unit_tests/tests_morse_receiver

TESTS += functional_tests/automatic/option_cwdevice_tty_lines/test_program
TESTS += functional_tests/automatic/option_port/test_program
TESTS += functional_tests/automatic/reset_register_callback/test_program
TESTS += functional_tests/automatic/request_caret/test_program
TESTS += functional_tests/automatic/request_esc_5/test_program
TESTS += functional_tests/automatic/request_esc_h/test_program

endif


