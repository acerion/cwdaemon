SUBDIRS=src doc test

AM_CFLAGS = @UNIXCW_CFLAGS@

CLEANFILES =  *~

CWDAEMON_EXAMPLE_FILES=examples/example.sh
CWDAEMON_TEST_FILES=test/cwtest.c test/Makefile.am test/Makefile.in
CWDAEMON_PATCHES=patches/cwdaemon-0.5-sergei.diff patches/cwdaemon-0.7-ok1zia.diff patches/cwdaemon-mt.patch
CWDAEMON_SCHEMATICS_FILES=doc/schematics/parallelport_circuit.jpg doc/schematics/parallelport_circuit.ps \
	doc/schematics/serialport_circuit.jpg doc/schematics/serialport_circuit.ps

EXTRA_DIST = cwdaemon.spec.in cwdaemon.spec \
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
	$(INSTALL_SCRIPT) $(srcdir)/cwsetup.sh $(DESTDIR)$(pkgdatadir)

	$(INSTALL_DATA) $(srcdir)/test/cwtest.c $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) $(srcdir)/cwdaemon.png $(DESTDIR)$(pkgdatadir)


uninstall-local:
	$(RM) $(DESTDIR)/$(pkgdatadir)/*


rpm: Makefile
	make dist
	$(RPMBUILD) -ta $(PACKAGE)-$(VERSION).tar.gz
	rm $(PACKAGE)-$(VERSION).tar.gz


