# https://www.gnu.org/software/automake/manual/html_node/Subdirectories-with-AM_005fCONDITIONAL.html
if CWDAEMON_TESTS
MAYBE_TESTS=tests
endif
SUBDIRS=src doc examples $(MAYBE_TESTS)


CLEANFILES = *~ \
	./tests/*~ \
	./tests/cwdaemon/*~ \
	./tests/cwdaemon/test/*~


CWDAEMON_EXAMPLE_FILES=README \
	examples/example.pl \
	examples/example.sh \
	examples/example.c examples/Makefile.am examples/Makefile.in

CWDAEMON_TEST_FILES=tests/cwtest.pl \
	tests/cwtest_esc2.pl \
	tests/cwtest_esc3.pl \
	tests/cwtest_esc5.pl tests/cwtest_esc5.sh \
	tests/cwtest_esc64.pl \
	tests/cwtest_esc7.pl \
	tests/cwtest_esc8.pl \
	tests/cwtest_escd.pl \
	tests/cwtest_escf.pl \
	tests/cwtest_escg.pl \
	tests/cwtest_short_space.pl \
	tests/cwdaemon/client.pm \
	tests/cwdaemon/test/common.pm

CWDAEMON_PATCHES=patches/cwdaemon-0.5-sergei.diff patches/cwdaemon-0.7-ok1zia.diff patches/cwdaemon-mt.patch
CWDAEMON_SCHEMATICS_FILES=doc/schematics/parallelport_circuit.jpg doc/schematics/parallelport_circuit.ps \
	doc/schematics/serialport_circuit.jpg doc/schematics/serialport_circuit.ps

EXTRA_DIST = cwdaemon.spec.in \
	$(CWDAEMON_EXAMPLE_FILES) \
	$(CWDAEMON_TEST_FILES) \
	$(CWDAEMON_CWDAEMON_PATCHES) \
	$(CWDAEMON_SCHEMATICS_FILES) \
	cwdaemon.png TODO README cwsetup.sh \
	debian \
	Doxyfile \
	tests_unit/README



install-data-local:
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) $(srcdir)/README $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) $(srcdir)/doc/schematics/parallelport_circuit.ps $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) $(srcdir)/doc/schematics/serialport_circuit.ps $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) $(srcdir)/doc/schematics/parallelport_circuit.jpg $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) $(srcdir)/doc/schematics/serialport_circuit.jpg $(DESTDIR)$(pkgdatadir)

        # The scripts will be installed as executable.
	$(INSTALL_SCRIPT) $(srcdir)/examples/example.sh $(DESTDIR)$(pkgdatadir)
	$(INSTALL_SCRIPT) $(srcdir)/examples/example.pl $(DESTDIR)$(pkgdatadir)
	$(INSTALL_SCRIPT) $(srcdir)/cwsetup.sh $(DESTDIR)$(pkgdatadir)

	$(INSTALL_DATA) $(srcdir)/examples/README $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) $(srcdir)/examples/example.c $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) $(srcdir)/examples/Makefile.am $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) $(srcdir)/examples/Makefile.in $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) $(srcdir)/cwdaemon.png $(DESTDIR)$(pkgdatadir)


uninstall-local:
	$(RM) $(DESTDIR)/$(pkgdatadir)/*


rpm: Makefile
	make dist
	$(RPMBUILD) -ta $(PACKAGE)-$(VERSION).tar.gz
	rm $(PACKAGE)-$(VERSION).tar.gz




# Currently "tests" dir contains higher-level tests. To distinguish dir with
# unit tests, I'm calling it "tests_unit".
#
# This target is meant to be executed quickly, it includes test cases that
# are completed in a fraction of a second.
.PHONY = check

UNIT_TESTS = tests_unit/utils
TESTS = $(UNIT_TESTS)
check_PROGRAMS = $(UNIT_TESTS)

tests_unit_utils_SOURCES = src/utils.c tests_unit/utils.c



