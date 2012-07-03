SUBDIRS=src doc test

INCLUDES = @UNIXCW_CFLAGS@

CLEANFILES =  *~

EXTRA_DIST = cwdaemon.spec.in cwdaemon.spec \
	test/cwtest.sh test/cwtest.c test/Makefile.am test/Makefile.in \
	patches/cwdaemon-0.5-sergei.diff patches/cwdaemon-0.7-ok1zia.diff patches/cwdaemon-mt.patch \
	doc/schematics/parallelport_circuit.jpg doc/schematics/parallelport_circuit.ps \
	doc/schematics/serialport_circuit.jpg doc/schematics/serialport_circuit.ps \
	cwdaemon.png TODO README cwsetup.sh \
	debian





rpm: Makefile
	make dist
	$(RPMBUILD) -ta $(PACKAGE)-$(VERSION).tar.gz
	rm $(PACKAGE)-$(VERSION).tar.gz


