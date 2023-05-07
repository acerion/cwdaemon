# Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
#		       and many authors, see the AUTHORS file.
# Copyright (C) 2012 - 2023 Kamil Ignacak (acerion@wp.pl)
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
cwdaemon_SOURCES = cwdaemon.c cwdaemon.h log.c log.h lp.c lp.h ttys.c null.c help.c help.h socket.c socket.h utils.c utils.h
# target-specific preprocessor flags (#defs and include dirs)
cwdaemon_CPPFLAGS = ${AM_CFLAGS} ${LIBCW_CFLAGS} #-I$(top_srcdir)/src/
# target-specific linker flags (objects to link)
cwdaemon_LDADD = ${LIBCW_LIBS}
