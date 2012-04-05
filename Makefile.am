INCLUDES = @UNIXCW_CFLAGS@

cwdaemon_LDADD = @UNIXCW_LIBS@

sbin_PROGRAMS = cwdaemon

CLEANFILES =  *~

EXTRA_DIST = cwdaemon.spec.in cwdaemon.spec test/cwtest.sh test/cwtest.c \
  cwdaemon.png TODO cwsetup.sh

man_MANS = cwdaemon.8

cwdaemon_SOURCES = cwdaemon.c cwdaemon.h lp.c ttys.c null.c

install-data-local:
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) README $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) schematics/parallelport_circuit.ps $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) schematics/serialport_circuit.ps $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) schematics/parallelport_circuit.jpg $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) schematics/serialport_circuit.jpg $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) test/cwtest.sh $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) cwsetup.sh $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) test/cwtest.c $(DESTDIR)$(pkgdatadir)
	$(INSTALL_DATA) cwdaemon.png $(DESTDIR)$(pkgdatadir)
 
dist-hook:
	if test -d schematics; then \
		mkdir $(distdir)/schematics; \
		for file in schematics/*; do \
			if test -f $$file; then \
				cp -p $$file $(distdir)/schematics; \
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
 

