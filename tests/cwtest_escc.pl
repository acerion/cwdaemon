#!/usr/bin/perl


# Test script for cwdaemon.
#
# This script tests cwdaemon's responses to <ESC>c escaped
# request. The request is used to play constant sound for n seconds
# (tune).


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
# GNU Library General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


use warnings;
use strict;

use IO::Socket::INET;
use IO::Handle;
use Getopt::Long;



use cwdaemon::test::common;



# These two values are taken from libcw.h
my $time_min = 0;
my $time_max = 10;

my $time_initial = 2;         # Initial valid value set before trying to set invalid values
my $time_invalid1 = 100;      # Simple invalid value
my $time_invalid2 = -1;       # Simple invalid value



my $request_code = 'c';   # Code of Escape request


my $cycles = 5;           # How many times to run a basic set of tests.
my $cycle = 0;
my $input_text = "   ";   # Text to be played after each tuning. We need to enqueue this test after tuning request to make loops work correctly.
my $delta = 1;            # Change (in seconds) per one step in a loop.


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
	print "Testing setting tuning time in valid range\n";
	cwdaemon::test::common::esc_set_initial_parameters($cwsocket);
	&cwdaemon_test_valid;

	print "\n";
    }


    if ($test_set =~ "i") {
	print "Testing setting tuning time in invalid range\n";
	cwdaemon::test::common::esc_set_initial_parameters($cwsocket);
	&cwdaemon_test_invalid;
    }
}


$cwsocket->close();








# ==========================================================================
#                                    subs
# ==========================================================================





# Testing setting tuning time in valid range
sub cwdaemon_test_valid
{
    # Time going from min to max
    for (my $time = $time_min; $time <= $time_max; $time += $delta) {

	print "    Setting time $time (up)\n";
	print $cwsocket chr(27).$request_code.$time;

	# Request made to wait for end of tuning before next iteration
	# of the loop.
	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;
    }


    # Time going from max to min
    for (my $time = $time_max; $time >= $time_min; $time -= $delta) {

	print "    Setting time $time (down)\n";
	print $cwsocket chr(27).$request_code.$time;

	# Request made to wait for end of tuning before next iteration
	# of the loop.
	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;
    }

    return;
}





# Testing setting invalid values of <ESC>c request
sub cwdaemon_test_invalid
{
    # Set an initial valid value as a preparation
    cwdaemon::test::common::esc_set_initial_valid_send($cwsocket, $request_code, $input_text, $time_initial);


    # Try setting a simple invalid value
    cwdaemon::test::common::esc_set_invalid_send($cwsocket, $request_code, $input_text, $time_invalid1);


    # Try setting a simple invalid value
    cwdaemon::test::common::esc_set_invalid_send($cwsocket, $request_code, $input_text, $time_invalid2);


    # Try setting value that is a bit too low and then value that is a
    # bit too high
    cwdaemon::test::common::esc_set_min1_max1_send($cwsocket, $request_code, $input_text, $time_min, $time_max);


    # Try setting 'out of range' long values
    cwdaemon::test::common::esc_set_oor_long_send($cwsocket, $request_code, $input_text);


    # Try setting invalid float values
    cwdaemon::test::common::esc_set_invalid_float_send($cwsocket, $request_code, $input_text);


    # Try setting 'not a number' values
    cwdaemon::test::common::esc_set_nan_send($cwsocket, $request_code, $input_text);


    return;
}
