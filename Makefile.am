SUBDIRS=src doc examples tests


CLEANFILES = *~ \
	./tests/*~ \
	./tests/cwdaemon/*~ \
	./tests/cwdaemon/test/*~


EXTRA_DIST = cwdaemon.spec.in \
	patches \
	doc/schematics \
	examples \
	cwdaemon.png TODO README cwsetup.sh \
	Doxyfile

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


clean-local:
if ENABLE_GCOV
	make gcov-clean
endif


rpm: Makefile
	make dist
	$(RPMBUILD) -ta $(PACKAGE)-$(VERSION).tar.gz
	rm $(PACKAGE)-$(VERSION).tar.gz


if ENABLE_GCOV

gcov:
	make clean
	make gcov-clean
	make gcov -C tests


gcov-clean:
	make gcov-clean -C src
	make gcov-clean -C tests

endif


