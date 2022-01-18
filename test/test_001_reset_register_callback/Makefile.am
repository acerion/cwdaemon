# Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
#		       and many authors, see the AUTHORS file.
# Copyright (C) 2012 - 2022 Kamil Ignacak (acerion@wp.pl)
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
check_PROGRAMS = test

# source code files used to build cwdaemon program
test_SOURCES = main.c key_source.c key_source_serial.c cw_rec_utils.c ../library/socket.c
# target-specific preprocessor flags (#defs and include dirs)
test_CPPFLAGS = ${AM_CFLAGS} ${LIBCW_CFLAGS}
# target-specific linker flags (objects to link)
test_LDADD = ${LIBCW_LIBS} -lpthread

test_LDFLAGS = -Wl,-rpath=${prefix}/lib

