.PHONY: check

SUBDIRS=unit_tests
if FUNCTIONAL_TESTS
SUBDIRS+=library
SUBDIRS+=test_001_basic_process_control
SUBDIRS+=test_002_reset_register_callback
SUBDIRS+=test_003_cwdevice_tty_line_options
endif


TESTS  = unit_tests/random
TESTS += unit_tests/utils
if FUNCTIONAL_TESTS
TESTS+=test_001_basic_process_control/test_program
TESTS+=test_002_reset_register_callback/test_program
TESTS+=test_003_cwdevice_tty_line_options/test_program
endif




