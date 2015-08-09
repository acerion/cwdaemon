#!/usr/bin/perl


# Test script for cwdaemon.
# 
# This script checks if cwdaemon is linking to libcw version that is
# affected by "short space" problem. This problem results in cwdaemon
# hanging on space character sent in some circumstance. The fix for
# this problem should be implemented in libcw, the test only checks
# that libcw has this fix implemented.


# Copyright (C) 2015 Kamil Ignacak
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
use Getopt::Long;




use cwdaemon::client;



# How long to sleep after every basic set of tests (the set is run in
# loop). Proper value of this variable is critical to be able to
# recreate the "short space" problem.
#
# Zero seconds seem to be too little. One second is enough to trigger
# the problem. Using two seconds to be sure that the problem will
# always be triggered.
my $in_loop_sleep = 2;



# How many times to run a basic set of tests.
my $cycles = 50;
my $cycle = 0; # Iterator.

my $input_text = "s ";



my $result = GetOptions("cycles=i"     => \$cycles,
			"input-text=s" => \$input_text)

    or die "Problems with getting options: $@\n";



my $server_port = 6789;
my $cwsocket = IO::Socket::INET->new(PeerAddr => "localhost",
				     PeerPort => $server_port,
				     Proto    => "udp",
				     Type     => SOCK_DGRAM)

    or die "Couldn't setup udp server on port $server_port : $@\n";



sub INT_handler {

    print "\n";

    $cwsocket->close();

    exit(0);

}

$SIG{'INT'} = 'INT_handler';





for ($cycle = 1; $cycle <= $cycles; $cycle++) {

    print("\nTest $cycle/$cycles\n");

    # PTT ON
    # print $cwsocket chr(27).'a1';

    cwtest_send($cwsocket, $input_text);
}


$cwsocket->close();




# ==========================================================================
#                                    subs
# ==========================================================================





# This function sends following request to the server:
# "<ESC>h<non-empty reply text>"
# "<single-character text to be played>"
#
# This function expects the following reply from the server:
# "h<non-empty reply text>\r\n"
sub cwtest_send
{
    my $cwsocket = shift;
    my $txt = shift;

    for (my $i = 0; $i < length($txt); $i++) {

	my $expected = "reply";
	my $request_prefix = "h";

	my $text = substr($txt, $i, 1);

	print "    sending  '" . $text . "'\n";
	if ($text eq " ") {
	    print "        cwdaemon linking to faulty libcw version will hang on this character\n";
	}

        # Use "<ESC>h" request to define reply expected from the server.
	print $cwsocket chr(27) . $request_prefix . $expected;
	# Send text to be played by server.
	print $cwsocket $text;

	my ($reply_prefix, $reply_text, $reply_postfix) = cwdaemon::client::receive($cwsocket, $request_prefix);

	if ($reply_text ne $expected) {
	    die "die 1, incorrect reply: '$reply_text' != '$expected'\n";
	}

	if ($text eq " ") {
	    # We managed to get here, so cwdaemon didn't hang on the
	    # space.
	    print "        success: cwdaemon is linking to libcw version that works around \"short space\" problem\n";
	}
	
	sleep $in_loop_sleep;
    }
}
