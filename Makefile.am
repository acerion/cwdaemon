SUBDIRS=src doc test

AM_CFLAGS = @UNIXCW_CFLAGS@

CLEANFILES =  *~

EXTRA_DIST = cwdaemon.spec.in cwdaemon.spec \
	test/cwtest.sh test/cwtest.c test/Makefile.am test/Makefile.in \
	patches/cwdaemon-0.5-sergei.diff patches/cwdaemon-0.7-ok1zia.diff patches/cwdaemon-mt.patch \
	doc/schematics/parallelport_circuit.jpg doc/schematics/parallelport_circuit.ps \
	doc/schematics/serialport_circuit.jpg doc/schematics/serialport_circuit.ps \
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
	$(INSTALL_SCRIPT) $(srcdir)/test/cwtest.sh $(DESTDIR)$(pkgdatadir)
	$(INSTALL_SCRIPT) $(srcdir)/cwsetup.sh $(DESTDIR)$(pkgdatadir)

	$(INSTALL_DATA) $(srcdir)/test/cwtest.c $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) $(srcdir)/cwdaemon.png $(DESTDIR)$(pkgdatadir)


uninstall-local:
	$(RM) $(DESTDIR)/$(pkgdatadir)/*


dist-hook:
	if test -d doc/schematics; then \
		mkdir $(distdir)/doc/schematics; \
		for file in doc/schematics/*; do \
			if test -f $$file; then \
				cp -p $$file $(distdir)/doc/schematics; \
			fi \
		done \
	fi
	if test -d test; then \
		mkdir $(distdir)/test; \
		for test in test/*; do \
			if test -f $$test; then \
				cp -p $$test $(distdir)/test; \
			fi \
		done \
	fi
	if test -d patches; then \
		mkdir $(distdir)/patches; \
		for patch in patches/*; do \
			if test -f $$patch; then \
				cp -p $$patch $(distdir)/patches; \
			fi \
		done \
	fi





rpm: Makefile
	make dist
	$(RPMBUILD) -ta $(PACKAGE)-$(VERSION).tar.gz
	rm $(PACKAGE)-$(VERSION).tar.gz


