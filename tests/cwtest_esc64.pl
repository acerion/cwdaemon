#!/usr/bin/perl


# Test script for cwdaemon.
#
# This script tests cwdaemon's responses to <ESC>6 (word mode) and
# <ESC>4 (abort message) escaped requests.


# Copyright (C) 2015 - 2024 Kamil Ignacak
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
use IO::Handle qw( );  # For flush


use lib "./";
use cwdaemon::test::common;


# Code of Escape request
my $word_mode_code = '6';
my $abort_code = '4';
my $reset_code = '0';


my $cycles = 3;               # How many times to run a basic set of tests.
my $cycle = 0;

# Text long enough for cwdaemon to start playing it, and to be able to
# abort playing it in the middle of the word.
my $input_text = "Loremipsumdolorsitame";         # Text to be played


my $test_set = "vi";      # Set of tests: valid and invalid parameter values


my $result = GetOptions("cycles=i"     => \$cycles,
			"input-text=s" => \$input_text,
			"test-set=s"   => \$test_set)

    or die "[EE] Problems with getting options: $@\n";



my $server_port = 6789;
my $cwsocket = IO::Socket::INET->new(PeerAddr => "localhost",
				     PeerPort => $server_port,
				     Proto    => "udp",
				     Type     => SOCK_DGRAM)
    or die "[EE] Couldn't setup udp server on port $server_port : $@\n";





sub INT_handler
{
    print "\n";
    $cwsocket->close();

    exit(0);
}

$SIG{'INT'} = 'INT_handler';





for ($cycle = 1; $cycle <= $cycles; $cycle++) {

    print "\n\n";
    print "[II] Cycle $cycle/$cycles\n";
    print "\n";


    if ($test_set =~ "v") {
	print "[II] Testing word mode and aborting message (VALID test set)\n\n";

	# Reset parameters to nominal
	print "[II] Setting normal (interruptible) mode:\n";
	print $cwsocket chr(27).$reset_code;

	print "[II] Setting initial parameter values:\n";
	cwdaemon::test::common::esc_set_initial_parameters($cwsocket);
	&cwdaemon_test_valid;

	print "\n\n";
    }


    if ($test_set =~ "i") {
	print "[II] Testing word mode and aborting message (INVALID test set)\n\n";

	# Reset parameters to nominal
	print "[II] Setting normal (interruptible) mode:\n";
	print $cwsocket chr(27).$reset_code;

	print "[II] Setting initial parameter values:\n";
	cwdaemon::test::common::esc_set_initial_parameters($cwsocket);
	&cwdaemon_test_invalid;

	print "\n";
    }
}


print "\n";
print "[II] End of test\n";
$cwsocket->close();




# ==========================================================================
#                                    subs
# ==========================================================================





# Testing '6' and '4' escaped requests (valid requests)
sub cwdaemon_test_valid
{
    &cwdaemon_test("");

    return;
}



# Testing '6' and '4' escaped requests (invalid requests)
#
# "Invalidity" of the requests is now limited to sending requests
# followed by some non-empty value. Requests '6' and '4' aren't
# supposed to have any value (for now), so let's see how cwdaemon
# handles a non-empty value.
sub cwdaemon_test_invalid
{
    &cwdaemon_test("k");

    return;
}




# Testing '6' and '4' escaped requests
sub cwdaemon_test
{
    my $suffix = shift;

    print "[II] Sending message in normal (interruptible) mode:\n";
    sleep(1);
    print $cwsocket $input_text."^";

    # Give cwdaemon some time to play beginning of a word.
    sleep(2);

    print "[II] Attempting to abort in normal (interruptible) mode... ";
    STDOUT->flush();
    sleep(1);
    print "now\n";
    print $cwsocket chr(27).$abort_code.$suffix;

    my $reply = <$cwsocket>;



    print "\n";
    sleep(3);




    print "[II] Setting word (non-interruptible) mode:\n";
    print $cwsocket chr(27).$word_mode_code.$suffix;
    sleep(1);


    print "[II] Sending message in word (non-interruptible) mode:\n";
    sleep(1);
    print $cwsocket $input_text."^";

    # Give cwdaemon some time to play beginning of a word.
    sleep(2);

    print "[II] Attempting to abort in word mode... ";
    STDOUT->flush();
    sleep(1);
    print "now\n";
    print $cwsocket chr(27).$abort_code.$suffix;

    $reply = <$cwsocket>;


    return;
}
