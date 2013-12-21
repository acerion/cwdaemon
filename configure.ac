# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(cwdaemon, 0.9.7, acerion@wp.pl)
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([src/cwdaemon.c])
AC_PROG_GCC_TRADITIONAL
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

#AC_CHECK_HEADERS(sys/kd.h sys/vtkd.h sys/kbio.h)

# getopt_long()
AC_CHECK_HEADERS([getopt.h])

AC_CHECK_HEADERS(libcw.h)
#AC_CHECK_HEADERS(sys/soundcard.h soundcard.h)
#if test "$ac_cv_header_sys_soundcard_h" = 'no' \
#	       && test "$ac_cv_header_soundcard_h" == 'no'; then
#	AC_MSG_ERROR(Cannot find sys/soundcard.h or soundcard.h)
#fi

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([bzero socket strerror])
AC_CHECK_FUNCS([setpriority])
# check for single precision versions of sin() and floor()
#AC_CHECK_LIB([m],[sinf],[AC_DEFINE([HAVE_SINF],[1],[libm includes sinf])])
#AC_CHECK_LIB([m],[floorf],[AC_DEFINE([HAVE_FLOORF],[1],[libm includes floorf])])
#AC_CHECK_LIB([m],[sin])
#AC_CHECK_LIB([ossaudio],[_oss_ioctl])

# New function in libcw5
AC_CHECK_LIB([cw], [cw_debug_get_flags], [AC_MSG_NOTICE([Found libcw5])], [AC_MSG_ERROR([Can't find libcw5 from unixcw package.])])


# For debug purposes only.
# CFLAGS=`echo "$CFLAGS" | sed 's/-O2/-O0/g'`

if test "${ac_cv_c_compiler_gnu}" = "yes"; then
  #CFLAGS="${CFLAGS} -Wall"
  CFLAGS="${CFLAGS} -Wall -W -pedantic -Wfloat-equal -Wpointer-arith -Wcast-qual -Wwrite-strings -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations -Wredundant-decls"
fi
CFLAGS="${CFLAGS} -std=c99"


PKG_CHECK_MODULES(UNIXCW, [libcw >= 4])
AC_SUBST(UNIXCW_CFLAGS)
AC_SUBST(UNIXCW_LIBS)


AC_OUTPUT([Makefile
	src/Makefile
	doc/Makefile
	test/Makefile
	doc/cwdaemon.8
	cwdaemon.spec])
