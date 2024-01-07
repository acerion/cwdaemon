.PHONY: check

SUBDIRS = unit_tests

TESTS  = unit_tests/random
TESTS += unit_tests/utils
TESTS += unit_tests/time_utils
TESTS += unit_tests/test_options


if FUNCTIONAL_TESTS
SUBDIRS += library
SUBDIRS += functional_tests

TESTS += functional_tests/automatic/option_cwdevice_tty_lines/test_program
TESTS += functional_tests/automatic/option_port/test_program
TESTS += functional_tests/automatic/reset_register_callback/test_program
TESTS += functional_tests/automatic/request_esc_5/test_program
TESTS += functional_tests/automatic/request_esc_h/test_program
endif




