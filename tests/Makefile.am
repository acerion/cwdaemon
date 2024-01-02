.PHONY: check

SUBDIRS = unit_tests

TESTS  = unit_tests/random
TESTS += unit_tests/utils
TESTS += unit_tests/time_utils


if FUNCTIONAL_TESTS
SUBDIRS += library
SUBDIRS += functional_tests
SUBDIRS += test_003_cwdevice_tty_line_options

TESTS += functional_tests/automatic/reset_register_callback/test_program
TESTS += test_003_cwdevice_tty_line_options/test_program
TESTS += functional_tests/automatic/request_esc_5/test_program
TESTS += functional_tests/automatic/request_esc_h/test_program
endif




