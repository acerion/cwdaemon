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




#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // strcasecmp()

#include "libcw.h"
#include "log.h"
#include "supervisor.h"
#include "test_options.h"




/// @file
///
/// Handling of command line options passed to test programs and of options
/// stored in dedicated ENV variables.




#define ENV_SOUND_SYSTEM   "CWDAEMON_TEST_SOUND_SYSTEM"
#define ENV_SUPERVISOR     "CWDAEMON_TEST_SUPERVISOR"




static struct option g_test_options_long[] = {
	{ "sound-system",    required_argument,       NULL, 'x' }, /* cwdaemon also uses "-x" for sound system. */
	{ "supervisor",      required_argument,       NULL, 's' },
	{ "random-seed",     required_argument,       NULL, 'r' },
	{ "help",            no_argument,             NULL, 'h' },
	{ 0,                 0,                       NULL,  0  }
};




static int get_sound_system(const char * value, enum cw_audio_systems * sound_system);
static int get_supervisor_id(const char * value, supervisor_id_t * supervisor_id);
static int get_options_from_env(test_options_t * test_opts);
static int get_options_from_args(int argc, char * const * argv, test_options_t * test_opts);
static void print_help(FILE * file);




int test_options_get(int argc, char * const * argv, test_options_t * test_opts)
{
	if (0 != get_options_from_env(test_opts)) {
		test_log_err("Test options: failed at getting options from env %s\n", "");
		return -1;
	}
	if (0 != get_options_from_args(argc, argv, test_opts)) {
		test_log_err("Test options: failed at getting options from args %s\n", "");
		return -1;
	}
	return 0;
}




/// @brief Parse shell env variables relevant to cwdaemon's test programs
///
/// Result of parsing is stored in @p test_opts
///
/// @reviewed_on{2024.04.19}
///
/// @param[out] test_opts Test options variable to fill
///
/// @return 0 on success (0 is also returned if env variables were not set)
/// @return -1 on failure
static int get_options_from_env(test_options_t * test_opts)
{
	char * value = NULL;

	value = getenv(ENV_SOUND_SYSTEM);
	if (value && strlen(value)) {
		if (0 != get_sound_system(value, &test_opts->sound_system)) {
			test_log_err("Test options: failed at getting sound system from env value string [%s]\n", value);
			return -1;
		}
	}

	value = getenv(ENV_SUPERVISOR);
	if (value && strlen(value)) {
		if (0 != get_supervisor_id(value, &test_opts->supervisor_id)) {
			test_log_err("Test options: failed at getting supervisor id from env value string [%s]\n", value);
			return -1;
		}
	}

	return 0;
}




/// @brief Parse command line options passed to cwdaemon's test programs
///
/// Result of parsing is stored in @p test_opts.
///
/// When "-h"/"--help" option is detected, function stops parsing the
/// remainder of options. Function then sets test_opts::invoked_help to true,
/// and returns 0.
///
/// @reviewed_on{2024.04.19}
///
/// @param[in] argc Test program's arguments count (main()'s "argc" argument)
/// @param[in] argv Test program's arguments vector (main*)'s "argv" argument)
/// @param[out] test_opts Test options variable to fill
///
/// @return 0 on success (0 is also returned if no options were passed)
/// @return -1 on failure
static int get_options_from_args(int argc, char * const * argv, test_options_t * test_opts)
{
	int option_index = 0;
	int c = 0;
	char * endptr = NULL;

	// In general we disallow short options, but for "help" we make an
	// exception.
	char const * short_options = "h";

	while ((c = getopt_long(argc, argv, short_options, g_test_options_long, &option_index)) != -1) {
		switch (c) {
		case 'x':
			if (0 != get_sound_system(optarg, &test_opts->sound_system)) {
				test_log_err("Test options: failed at getting sound system from optarg value string [%s]\n", optarg);
				return -1;
			}
			break;
		case 's':
			if (0 != get_supervisor_id(optarg, &test_opts->supervisor_id)) {
				test_log_err("Test options: failed at getting supervisor id from optarg value string [%s]\n", optarg);
				return -1;
			}
			break;
		case 'r':
			test_opts->random_seed = (uint32_t) strtoul(optarg, &endptr, 10);
			if (endptr && strlen(endptr) > 0) {
				test_log_err("Test options: failed at getting seed from optarg value string [%s] (endptr = [%s])\n", optarg, endptr);
				return -1;
			}
			break;
		case 'h':
			print_help(stderr);
			test_opts->invoked_help = true;
			return 0; /* Don't process further options. */
		default:
			test_log_err("Test options: Unhandled option '%c' / %d\n", c, c);
			print_help(stderr);
			return -1;
		};
	}

	return 0;
}




/// @brief Parse given string as name of one of sound systems
///
/// @reviewed_on{2024.04.19}
///
/// @param[in] value String to be parsed
/// @param[out] sound_system Sound system parsed from @p value
///
/// @return 0 on success
/// @return -1 on failure
static int get_sound_system(const char * value, enum cw_audio_systems * sound_system)
{
	if (0 == strcasecmp(value, "null")) {
		*sound_system = CW_AUDIO_NULL;
	} else if (0 == strcasecmp(value, "console")) {
		*sound_system = CW_AUDIO_CONSOLE;
	} else if (0 == strcasecmp(value, "oss")) {
		*sound_system = CW_AUDIO_OSS;
	} else if (0 == strcasecmp(value, "alsa")) {
		*sound_system = CW_AUDIO_ALSA;
	} else if (0 == strcasecmp(value, "pulseaudio")) {
		*sound_system = CW_AUDIO_PA;
	} else if (0 == strcasecmp(value, "soundcard")) {
		*sound_system = CW_AUDIO_SOUNDCARD;
	} else {
		test_log_err("Test options: failed at getting sound system from value string [%s]\n", value);
		return -1;
	}

	return 0;
}




/// @brief Parse given string as name of one of supervisor programs
///
/// @reviewed_on{2024.04.19}
///
/// @param[in] value String to be parsed
/// @param[out] supervisor_id ID of supervisor parsed from @p value
///
/// @return 0 on success
/// @return -1 on failure
static int get_supervisor_id(const char * value, supervisor_id_t * supervisor_id)
{
	if (0 == strcasecmp(value, "none")) {
		*supervisor_id = supervisor_id_none;
	} else if (0 == strcasecmp(value, "valgrind")) {
		*supervisor_id = supervisor_id_valgrind;
	} else if (0 == strcasecmp(value, "gdb")) {
		*supervisor_id = supervisor_id_gdb;
	} else {
		test_log_err("Test options: failed at getting supervisor id from value string [%s]\n", value);
		return -1;
	}

	return 0;
}




/// @brief Print test program's help text to given @p file
///
/// @reviewed_on{2024.04.19}
///
/// @param file File to which the help text will be printed
static void print_help(FILE * file)
{
	//               10         20       30        40        50        60        70
	/*       1234567890123456789012345678901234567890123456789012345678901234567890123456 */
	fprintf(file,
	        "This test program can be controlled by environment variables\n"
	        "and command line options.\n"
	        "Command line options have priority over environment variables.\n"
	        "\n");

	fprintf(file,
	        "-h/--help            Print this help text.\n"
	        "\n");

	fprintf(file,
	        "--sound-system <x>   Specify sound system to be used by cwdaemon.\n"
	        "                     One of: null, console, oss, alsa, pulseaudio, soundcard.\n"
	        "\n");

	fprintf(file,
	        "--supervisor <x>     Specify supervisor of cwdaemon.\n"
	        "                     One of: none, valgrind, gdb.\n"
	        "\n");

	fprintf(file,
	        "--random-seed <x>    Specify random seed to be used by test.\n"
	        "                     Pass an integer as value of this option.\n"
	        "\n");

	fprintf(file,
	        "Supported environment variables are:\n"
	        "\n");

	fprintf(file,
	        ENV_SOUND_SYSTEM "\n"
	        "                     Accepted values: see '--sound-system' option above.\n"
	        "\n");

	fprintf(file,
	        ENV_SUPERVISOR "\n"
	        "                     Accepted values: see '--supervisor' option above.\n"
	        "\n");

	return;
}

