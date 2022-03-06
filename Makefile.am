SUBDIRS=src doc examples
if CWDAEMON_TESTS
SUBDIRS+=test
endif

#AM_CFLAGS = @UNIXCW_CFLAGS@

CLEANFILES = *~ \
	./test/*~ \
	./test/cwdaemon/*~ \
	./test/cwdaemon/test/*~


CWDAEMON_EXAMPLE_FILES=README \
	examples/example.pl \
	examples/example.sh \
	examples/example.c examples/Makefile.am examples/Makefile.in

CWDAEMON_TEST_FILES=test/cwtest.pl \
	test/cwtest_esc2.pl \
	test/cwtest_esc3.pl \
	test/cwtest_esc5.pl test/cwtest_esc5.sh \
	test/cwtest_esc64.pl \
	test/cwtest_esc7.pl \
	test/cwtest_esc8.pl \
	test/cwtest_escd.pl \
	test/cwtest_escf.pl \
	test/cwtest_escg.pl \
	test/cwtest_short_space.pl \
	test/cwdaemon/client.pm \
	test/cwdaemon/test/common.pm

CWDAEMON_PATCHES=patches/cwdaemon-0.5-sergei.diff patches/cwdaemon-0.7-ok1zia.diff patches/cwdaemon-mt.patch
CWDAEMON_SCHEMATICS_FILES=doc/schematics/parallelport_circuit.jpg doc/schematics/parallelport_circuit.ps \
	doc/schematics/serialport_circuit.jpg doc/schematics/serialport_circuit.ps

EXTRA_DIST = cwdaemon.spec.in \
	$(CWDAEMON_EXAMPLE_FILES) \
	$(CWDAEMON_TEST_FILES) \
	$(CWDAEMON_CWDAEMON_PATCHES) \
	$(CWDAEMON_SCHEMATICS_FILES) \
	cwdaemon.png TODO README cwsetup.sh \
	debian



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
