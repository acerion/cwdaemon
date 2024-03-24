#!/usr/bin/perl


# Test script for cwdaemon.
#
# This script tests cwdaemon's responses to <ESC>7 escaped
# request. The request is used to modify weighting of Morse code sent
# by cwdaemon.


# Copyright (C) 2015 - 2023 Kamil Ignacak
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


use warnings;
use strict;

use IO::Socket::INET;
use IO::Handle;
use Getopt::Long;



use lib "./";
use cwdaemon::test::common;



# These two values are taken from libcw.h
my $weight_min = -50;
my $weight_max = 50;

my $speed_initial = 15;        # It's better to hear changing weight at lower speeds

my $weight_invalid1 = -55;     # Simple invalid value
my $weight_invalid2 = 220;     # Simple invalid value



my $request_code = '7';   # Code of Escape request


my $cycles = 5;           # How many times to run a basic set of tests.
my $cycle = 0;
my $input_text = ".";     # Text to be played. '.' = ".-.-.-", a good pattern to test weight changes
my $delta = 5;            # Change per one step in a loop.


my $test_set = "vi";      # Set of tests: valid and invalid parameter values


my $result = GetOptions("cycles=i"     => \$cycles,
			"input-text=s" => \$input_text,
			"delta=i"      => \$delta,
			"test-set=s"   => \$test_set)

    or die "Problems with getting options: $@\n";





if (!($test_set =~ "v")
    && !($test_set =~ "i")) {

    exit;
}





my $server_port = 6789;
my $cwsocket = IO::Socket::INET->new(PeerAddr => "localhost",
				     PeerPort => $server_port,
				     Proto    => "udp",
				     Type     => SOCK_DGRAM)
    or die "Couldn't setup udp server on port $server_port : $@\n";





sub INT_handler
{
    print "\n";
    $cwsocket->close();

    exit(0);
}

$SIG{'INT'} = 'INT_handler';





for ($cycle = 1; $cycle <= $cycles; $cycle++) {

    print "\n\n";
    print "Cycle $cycle/$cycles\n";
    print "\n";


    if ($test_set =~ "v") {
	print "Testing setting weight in valid range\n";

	cwdaemon::test::common::esc_set_initial_parameters($cwsocket);
	cwdaemon::test::common::esc_set_initial_valid_send($cwsocket, 2, $input_text, $speed_initial);

	print "\n";
	sleep 1;

	&cwdaemon_test_valid;

	print "\n";
    }


    if ($test_set =~ "i") {
	print "Testing setting weight in invalid range\n";

	cwdaemon::test::common::esc_set_initial_parameters($cwsocket);
	cwdaemon::test::common::esc_set_initial_valid_send($cwsocket, 2, $input_text, $speed_initial);

	print "\n";
	sleep 1;

	&cwdaemon_test_invalid;
    }
}


$cwsocket->close();








# ==========================================================================
#                                    subs
# ==========================================================================





# Testing setting weight in valid range
sub cwdaemon_test_valid
{
    # Weight going from min to max
    for (my $weight = $weight_min; $weight <= $weight_max; $weight += $delta) {

	print "    Setting weight $weight (up)\n";
	print $cwsocket chr(27).$request_code.$weight;

	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;
    }


    # Weight going from max to min
    for (my $weight = $weight_max; $weight >= $weight_min; $weight -= $delta) {

	print "    Setting weight $weight (down)\n";
	print $cwsocket chr(27).$request_code.$weight;

	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;
    }

    return;
}





# Testing setting invalid values of <ESC>7 request
sub cwdaemon_test_invalid
{
    # Try setting a simple invalid value
    cwdaemon::test::common::esc_set_invalid_send($cwsocket, $request_code, $input_text, $weight_invalid1);


    # Try setting a simple invalid value
    cwdaemon::test::common::esc_set_invalid_send($cwsocket, $request_code, $input_text, $weight_invalid2);


    # Try setting value that is a bit too low and then value that is a
    # bit too high
    cwdaemon::test::common::esc_set_min1_max1_send($cwsocket, $request_code, $input_text, $weight_min, $weight_max);


    # Try setting 'out of range' long values
    cwdaemon::test::common::esc_set_oor_long_send($cwsocket, $request_code, $input_text);


    # Try setting invalid float values
    cwdaemon::test::common::esc_set_invalid_float_send($cwsocket, $request_code, $input_text);


    # Try setting 'not a number' values
    cwdaemon::test::common::esc_set_nan_send($cwsocket, $request_code, $input_text);


    return;
}
