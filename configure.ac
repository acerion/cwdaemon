# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(cwdaemon, 0.12.0, acerion@wp.pl)
AM_INIT_AUTOMAKE([subdir-objects])
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([src/cwdaemon.c])
AC_PROG_GCC_TRADITIONAL
AC_PROG_RANLIB # Because of utility library built in in tests/library/
AC_CONFIG_HEADERS([config.h])
PKG_PROG_PKG_CONFIG # Don't call pkg-config directly. Use $PKG_CONFIG.


# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_CHECK_PROGS(RPMBUILD, rpmbuild, rpm)
AM_PROG_CC_C_O # per-target flags in src/Makefile.am

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([errno.h arpa/inet.h netinet/in.h fcntl.h netdb.h netinet/in.h \
stdlib.h string.h strings.h sys/ioctl.h sys/socket.h syslog.h unistd.h \
sys/time.h time.h linux/ppdev.h stdio.h signal.h stdarg.h termios.h sys/resource.h \
dev/ppbus/ppi.h sys/stat.h])

# AC_HEADER_TIME

# getopt_long()
AC_CHECK_HEADERS([getopt.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([socket strerror setpriority])


# Needed to have access to local cwdaemon binary that will be tested during
# "make check". "local" meaning a binary that has not been put into
# $prefix/sbin yet.
AC_SUBST([ABS_TOP_BUILDDIR], [abs_top_builddir])



# This is just a default value that can be overriden later.
default_test_cwdevice_name="ttyUSB0"



AC_CANONICAL_HOST
case $host_os in
    *linux*|*Linux*)
	AC_MSG_NOTICE([Detected Linux: $host_os])
	default_test_cwdevice_name="ttyUSB0"
	;;

	*freebsd*|*FreeBSD*)
	AC_MSG_NOTICE([Detected FreeBSD: $host_os])

	# Found on FreeBSD 13.2; TODO: double-check; user group: dialer
	default_test_cwdevice_name="cuau0"
	;;

	*openbsd*|*OpenBSD*)
	AC_MSG_NOTICE([Detected OpenBSD: $host_os])
	;;

	*netbsd*|*NetBSD*)
	# I didn't test unixcw on NetBSD, but let's give it a
	# chance.
	AC_MSG_NOTICE([Detected NetBSD: $host_os])
	;;

	*)
	AC_MSG_NOTICE([Detected other OS: $host_os])
	;;
esac


# AC_SUBST puts the TEST_CWDEVICE_NAME symbol into Makefiles
AC_SUBST([TEST_CWDEVICE_NAME], ${default_test_cwdevice_name})



# For debug purposes only.
# CFLAGS=`echo "$CFLAGS" | sed 's/-O2/-O0/g'`

if test "${ac_cv_c_compiler_gnu}" = "yes"; then
  CFLAGS="${CFLAGS} -Wall -Wextra -pedantic"
  CFLAGS="${CFLAGS} -Wswitch-default -Wimplicit-function-declaration -Wswitch-enum"
  CFLAGS="${CFLAGS} -Wfloat-equal -Wpointer-arith -Wcast-qual -Wwrite-strings"
  CFLAGS="${CFLAGS} -Wmissing-prototypes -Wstrict-prototypes"
  CFLAGS="${CFLAGS} -Wmissing-declarations -Wredundant-decls"

  # https://kristerw.blogspot.com/2017/09/useful-gcc-warning-options-not-enabled.html
  CFLAGS="${CFLAGS} -Wduplicated-cond -Wduplicated-branches -Wlogical-op"
  CFLAGS="${CFLAGS} -Wrestrict -Wnull-dereference -Wjump-misses-init -Wdouble-promotion"
  CFLAGS="${CFLAGS} -Wshadow -Wformat=2"
fi
CFLAGS="${CFLAGS} -std=c99"




# Build tests? No by default.
AC_ARG_ENABLE(cwdaemon_tests,
    AS_HELP_STRING([--enable-cwdaemon-tests], [enable cwdaemon tests]),
    [],
    [enable_cwdaemon_tests=no])

AC_MSG_CHECKING([whether to build cwdaemon tests])
if test x$enable_cwdaemon_tests = xyes ; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([CWDAEMON_TESTS], [test x$enable_cwdaemon_tests = xyes])



# LIBCW_LIBDIR is needed to extend value of LD_LIBRARY_PATH used when starting
# test instance of cwdaemon.
if $PKG_CONFIG --atleast-version=5 libcw; then
   LIBCW_LIBS=`$PKG_CONFIG libcw --libs`
   LIBCW_CFLAGS=`$PKG_CONFIG libcw --cflags`
   LIBCW_LIBDIR=`$PKG_CONFIG libcw --variable=libdir`
else
   AC_MSG_ERROR(Can't find libcw library)
fi
AC_SUBST(LIBCW_LIBS)
AC_SUBST(LIBCW_CFLAGS)
AC_SUBST(LIBCW_LIBDIR)




AC_CONFIG_FILES([Makefile
	src/Makefile
	doc/Makefile
	examples/Makefile
	doc/cwdaemon.8
	cwdaemon.spec
	tests/Makefile
	tests/library/Makefile
	tests/test_001_basic_process_control/Makefile
	tests/test_002_reset_register_callback/Makefile
	tests/test_003_cwdevice_tty_line_options/Makefile
	])
AC_OUTPUT



# Put this at the very end, so that these are the last lines seen by
# user on console with low number of rows.
AC_MSG_NOTICE([----------------------------------------------------------])
AC_MSG_NOTICE([---------- $PACKAGE_NAME $PACKAGE_VERSION build configuration -----------])
AC_MSG_NOTICE([  operating system:  ........  $host_os])
AC_MSG_NOTICE([  libcw library path:  ......  $LIBCW_LIBDIR])
AC_MSG_NOTICE([  libcw library version: ....  $(pkg-config --modversion libcw)])
AC_MSG_NOTICE([  LIBCW_LIBS:  ..............  $LIBCW_LIBS])
AC_MSG_NOTICE([  LIBCW_CFLAGS: .............  $LIBCW_CFLAGS])
AC_MSG_NOTICE([  CFLAGS:  ..................  $CFLAGS])
if test x$enable_cwdaemon_tests = xyes ; then
    AC_MSG_NOTICE([  enable tests: .............  yes])
    AC_MSG_NOTICE([  test cwdevice name: .......  $TEST_CWDEVICE_NAME])
fi
AC_MSG_NOTICE([----------------------------------------------------------])
