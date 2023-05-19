SUBDIRS=src doc examples tests


CLEANFILES = *~ \
	./tests/*~ \
	./tests/cwdaemon/*~ \
	./tests/cwdaemon/test/*~

EXTRA_DIST = cwdaemon.spec.in \
	examples \
	tests \
	patches \
	doc/schematics \
	cwdaemon.png TODO README cwsetup.sh \
	debian \
	Doxyfile \
	tests/unit_tests/README

dist-hook:
	find $(distdir) -type d -name ".deps" | xargs rm -rf


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



