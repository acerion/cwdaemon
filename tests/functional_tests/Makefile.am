.PHONY: check                                     \
	automatic/option_cwdevice_tty_lines.dummy     \
	automatic/option_port.dummy                   \
	automatic/request_caret.dummy                 \
	automatic/request_esc_5.dummy                 \
	automatic/request_esc_h.dummy                 \
	automatic/reset_register_callback.dummy


SUBDIRS = automatic




if ENABLE_GCOV

# Target for building coverage report for functional tests.
gcov:
	make automatic/option_cwdevice_tty_lines.gcov
	make automatic/option_port.gcov
	make automatic/request_caret.gcov
	make automatic/request_esc_5.gcov
	make automatic/request_esc_h.gcov
	make automatic/reset_register_callback.gcov


# POSIX-compliant "suffix rules" are SO MUCH FUN!
SUFFIXES=.gcov .dummy
.dummy.gcov: gcov_clean_old_artifacts
# Be careful to not remove entire OUTPUT_DIR, in which reports of other tests
# are stored.
	@echo "Coverage: removing old artifacts before building functional test [$*]"
	make clean -C $*
	rm -rf $(OUTPUT_DIR)/functional_tests/$*

	@echo "Coverage: building functional test [$*]"
	make check -C $*

	@echo "Coverage: running functional test [$*]"
	./$*/test_program

	@echo "Coverage: building report for functional test [$*]"
	mkdir -p $(OUTPUT_DIR)/functional_tests/$*
	lcov -directory $(abs_top_builddir)/src/ --capture --output-file $(OUTPUT_DIR)/functional_tests/$*/traceinfo.info
	genhtml -t $* -o $(OUTPUT_DIR)/functional_tests/$* $(OUTPUT_DIR)/functional_tests/$*/traceinfo.info

endif

