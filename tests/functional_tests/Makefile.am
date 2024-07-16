.PHONY: check gcov gcov2

SUBDIRS = unattended supervised




if ENABLE_GCOV

# Target for building coverage report for functional tests.
gcov:
	make gcov2 target=unattended/option_cwdevice_tty_lines
	make gcov2 target=unattended/option_port
	make gcov2 target=unattended/request_caret
	make gcov2 target=unattended/request_esc_exit
	make gcov2 target=unattended/request_esc_cwdevice
	make gcov2 target=unattended/request_esc_reply
	make gcov2 target=unattended/request_plain
	make gcov2 target=unattended/reset_register_callback



gcov2:
# Be careful to not remove entire OUTPUT_DIR, in which reports of other tests
# are stored.
	@echo "[II] Coverage: removing old artifacts before building functional test [$(target)]"
	make clean -C $(target)
	rm -rf $(OUTPUT_DIR)/functional_tests/$(target)

	@echo "[II] Coverage: building functional test [$(target)]"
	make check -C $(target)

	@echo "[II] Coverage: running functional test [$(target)]"
	./$(target)/test_program

	@echo "[II] Coverage: building report for functional test [$(target)]"
	mkdir -p $(OUTPUT_DIR)/functional_tests/$(target)
	lcov -directory $(abs_top_builddir)/src/ --capture --output-file $(OUTPUT_DIR)/functional_tests/$(target)/traceinfo.info
	genhtml -t $(target) -o $(OUTPUT_DIR)/functional_tests/$(target) $(OUTPUT_DIR)/functional_tests/$(target)/traceinfo.info

endif

