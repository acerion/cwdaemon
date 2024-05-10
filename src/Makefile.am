# Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
#		       and many authors, see the AUTHORS file.
# Copyright (C) 2012 - 2024 Kamil Ignacak (acerion@wp.pl)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

# program(s) to be built in current dir
sbin_PROGRAMS = cwdaemon

# source code files used to build cwdaemon program
cwdaemon_SOURCES = cwdaemon.c cwdaemon.h log.c log.h lp.c lp.h ttys.c ttys.h null.c help.c help.h \
                   options.c options.h \
                   sleep.c sleep.h \
                   socket.c socket.h utils.c utils.h

# target-specific preprocessor flags (#defs and include dirs)
cwdaemon_CPPFLAGS = ${AM_CFLAGS} ${LIBCW_CFLAGS}

# Target-specific linker flags (objects to link). Order is important: first
# static libraries then dynamic. Otherwise linker may not find symbols from
# the dynamic library.
cwdaemon_LDADD = ${LIBCW_LIBS}




if ENABLE_GCOV
cwdaemon_LDFLAGS = --coverage
cwdaemon_CFLAGS  = --coverage

gcov:
	echo "Empty gcov target in src/"

gcov-clean:
	rm -f *.gcda *.gcno
endif

