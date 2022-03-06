# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(cwdaemon, 0.10.2, acerion@wp.pl)
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([src/cwdaemon.c])
AC_PROG_GCC_TRADITIONAL
AC_PROG_RANLIB # Because of utility library built in in test/library/
AC_CONFIG_HEADERS([config.h])


# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_CC_STDC
AC_CHECK_PROGS(RPMBUILD, rpmbuild, rpm)
AM_PROG_CC_C_O # per-target flags in src/Makefile.am

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC

AC_CHECK_HEADERS([errno.h arpa/inet.h fcntl.h netdb.h netinet/in.h \
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
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([socket strerror])
AC_CHECK_FUNCS([setpriority])


# For debug purposes only.
# CFLAGS=`echo "$CFLAGS" | sed 's/-O2/-O0/g'`

if test "${ac_cv_c_compiler_gnu}" = "yes"; then
  CFLAGS="${CFLAGS} -Wall -Wextra -pedantic"
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




PKG_CHECK_MODULES(LIBCW, [libcw >= 5], [
			   AC_SUBST(LIBCW_CFLAGS)
			   AC_SUBST(LIBCW_LIBS)
			   AC_MSG_RESULT([    libcw CLFAGS is ${LIBCW_CFLAGS}])
			   AC_MSG_RESULT([    libcw LIBS is ${LIBCW_LIBS}])
			   ],
			   [AC_MSG_WARN([cannot find libcw pkgconfig information - unable to build cwdaemon])])


AC_OUTPUT([Makefile
	src/Makefile
	doc/Makefile
	examples/Makefile
	doc/cwdaemon.8
	cwdaemon.spec])

if test x$enable_cwdaemon_tests = xyes ; then
   AC_OUTPUT([
	test/Makefile
	test/library/Makefile
	test/test_001_basic_process_control/Makefile
	test/test_002_reset_register_callback/Makefile
	])
fi

