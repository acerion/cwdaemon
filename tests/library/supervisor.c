/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2024 Kamil Ignacak <acerion@wp.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   @file

   Supervisor: a program monitoring execution of test cwdaemon instance.

   cwdaemon is being executed inside of supervisor. The name may not be the
   best one, but I can't come up with anything better.

   The test program that is a parent process of test instance of cwdaemon
   process is not considered a supervisor (although it could be considered as
   such).

   If supervisor is set to something other than "none", then the test program
   is a parent process of supervisor, and supervisor runs the test instance
   of cwdaemon process.

   Examples of supervisors:
    - valgrind,
    - gdb (doesn't work correctly yet),

   The concept of supervisor was introduced because I need to be able to run
   any functional test while cwdaemon is being observed by valgrind or gdb.

   In theory I could manually start valgrind/gdb with test instance of
   cwdaemon process and then somehow run functional tests on that process,
   but it would not be convenient. The functional tests must pass different
   command-line options to cwdaemon, must know network port on which cwdaemon
   is listening, and sometimes also know PID of cwdaemon. It would be even
   more inconvenient in case of tests that require multiple starts/stops of
   cwdaemon process (e.g. tests of EXIT escaped requests, or tests of command
   line options).

   Thanks to having supervisor integrated into this test framework, I can
   just run a test binary in usual way and have a cwdaemon process running in
   valgrind, with all test-case-specific options passed to cwdaemon in
   command line, and with PID and network port known to the test binary.

   In order to use the supervisor, you just have to explicitly assign a value
   to `supervisor_id` member of `server_options_t` variable before passing
   the variable to server_start().
*/




#include "supervisor.h"




/*
  TODO acerion 2024.02.18: we should somehow control whether we don't put too
  many items into argv.
*/




int append_options_valgrind(const char ** argv, int * argc)
{
	argv[(*argc)++] = "valgrind";
	argv[(*argc)++] = "-s";
	argv[(*argc)++] = "--leak-check=full";
	argv[(*argc)++] = "--show-leak-kinds=all";

	return 0;
}




int append_options_gdb(const char ** argv, int * argc)
{
	argv[(*argc)++] = "gdb";
	argv[(*argc)++] = "--args";

	return 0;
}




