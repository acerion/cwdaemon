#!/usr/bin/perl


# Test script for cwdaemon.


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


# This script tests cwdaemon's responses to <ESC>3 escaped
# request. The request is used to modify tone of Morse code (Hz).



use warnings;
use strict;

use IO::Socket::INET;
use IO::Handle;
use Getopt::Long;



use cwdaemon::test::common;



# Values are taken from libcw.h
my $tone_min = 0;
my $tone_max = 4000;



# How many times to run a basic set of tests.
my $cycles = 5;
my $cycle = 0;


my $input_text = "s";


my $delta = 50;   # Change (in Hz) per one step in a loop.


my $result = GetOptions("cycles=i"     => \$cycles,
			"input_text=s" => \$input_text,
			"delta=i"      => \$delta)

    or die "Problems with getting options: $@\n";





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



cwdaemon::test::common::set_initial_parameters($cwsocket);



for ($cycle = 1; $cycle <= $cycles; $cycle++) {

    print "\n\n";

    print "Cycle $cycle/$cycles\n";

    print "\n";

    print "Testing setting tone in valid range\n";
    &cwdaemon_test0;

    print "\n";

    print "Testing setting invalid values\n";
    &cwdaemon_test1;
}


$cwsocket->close();








# ==========================================================================
#                                    subs
# ==========================================================================





# Testing setting tone in valid range
sub cwdaemon_test0
{
    # Tone going from min to max
    for (my $tone = $tone_min; $tone <= $tone_max; $tone += $delta) {

	print "    Setting tone $tone (up)\n";
	print $cwsocket chr(27).'3'.$tone;

	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;
    }


    # Tone going from max to min
    for (my $tone = $tone_max; $tone >= $tone_min; $tone -= $delta) {

	print "    Setting tone $tone (down)\n";
	print $cwsocket chr(27).'3'.$tone;

	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;
    }

    return;
}





# Testing setting invalid values of <ESC>3 request
sub cwdaemon_test1
{
    my $valid_tone = 300;

    # First set a valid tone
    print "    Setting initial valid tone $valid_tone\n";
    print $cwsocket chr(27).'3'.$valid_tone;

    print $cwsocket $input_text."^";
    my $reply = <$cwsocket>;




    # Then try to set tone that is too low
    my $invalid_tone = $tone_min - 1;

    print "    Trying to set invalid low tone $invalid_tone\n";
    print $cwsocket chr(27).'3'.$invalid_tone;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;




    # Then try to set tone that is too low
    $invalid_tone = $tone_max + 1;

    print "    Trying to set invalid high tone $invalid_tone\n";
    print $cwsocket chr(27).'3'.$invalid_tone;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;




    # Try to set totally invalid tone
    $invalid_tone = -1;

    print "    Trying to set negative tone $invalid_tone\n";
    print $cwsocket chr(27).'3'.$invalid_tone;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;




    # Another attempt at totally invalid tone
    $invalid_tone = $tone_max * 100000;

    print "    Trying to set very large tone $invalid_tone\n";
    print $cwsocket chr(27).'3'.$invalid_tone;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;




    # Now something that is not a number
    $invalid_tone = "k";

    print "    Trying to set completely invalid value $invalid_tone\n";
    print $cwsocket chr(27).'3'.$invalid_tone;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;

    return;
}
