#!/usr/bin/perl


# cwtest_esc2.pl - test script for cwdaemon
# Copyright (C) 2012 - 2014 Kamil Ignacak
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
#


#
# This script tests cwdaemon's responses to <ESC>2 escaped
# request. The request is used to modify speed of sending Morse code
# (wpm).
#



use warnings;
use strict;

use IO::Socket::INET;
use IO::Handle;
use Getopt::Long;




my $speed_min = 4;
my $speed_max = 60;



# How many times to run a basic set of tests.
my $cycles = 5;
my $cycle = 0;


my $input_text = "p";


my $result = GetOptions("cycles=i"     => \$cycles,
			"input_text=s" => \$input_text)

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





for ($cycle = 1; $cycle <= $cycles; $cycle++) {

    print "\n\n";

    print "Cycle $cycle/$cycles\n";

    print "\n";

    print "Testing setting speed in valid range\n";
    &cwdaemon_test0;

    print "\n";

    print "Testing setting invalid values\n";
    &cwdaemon_test1;
}


$cwsocket->close();








# ==========================================================================
#                                    subs
# ==========================================================================





# Testing setting speed in valid range
sub cwdaemon_test0
{
    # Speed going from min to max
    for (my $speed = $speed_min; $speed <= $speed_max; $speed++) {

	print "    Setting speed $speed (up)\n";
	print $cwsocket chr(27).'2'.$speed;

	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;
    }


    # Speed going from max to mi
    for (my $speed = $speed_max; $speed >= $speed_min; $speed--) {

	print "    Setting speed $speed (down)\n";
	print $cwsocket chr(27).'2'.$speed;

	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;
    }

    return;
}





# Testing setting invalid values of <ESC>2 request
sub cwdaemon_test1
{
    my $valid_speed = 10;

    # First set a valid speed
    print "    Setting initial valid speed $valid_speed\n";
    print $cwsocket chr(27).'2'.$valid_speed;

    print $cwsocket $input_text."^";
    my $reply = <$cwsocket>;




    # Then try to set speed that is too low
    my $invalid_speed = $speed_min - 1;

    print "    Trying to set invalid low speed $invalid_speed\n";
    print $cwsocket chr(27).'2'.$invalid_speed;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;




    # Then try to set speed that is too low
    $invalid_speed = $speed_max + 1;

    print "    Trying to set invalid high speed $invalid_speed\n";
    print $cwsocket chr(27).'2'.$invalid_speed;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;




    # Try to set a special case: zero (well, nothing really special
    # there, it's just an interesting value, let's see what happens).
    $invalid_speed = 0;

    print "    Trying to set zero speed $invalid_speed\n";
    print $cwsocket chr(27).'2'.$invalid_speed;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;




    # Try to set totally invalid speed
    $invalid_speed = -1;

    print "    Trying to set negative speed $invalid_speed\n";
    print $cwsocket chr(27).'2'.$invalid_speed;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;




    # Another attempt at totally invalid speed
    $invalid_speed = $speed_max * 100000;

    print "    Trying to set very large speed $invalid_speed\n";
    print $cwsocket chr(27).'2'.$invalid_speed;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;




    # Now something that is not a number
    $invalid_speed = "k";

    print "    Trying to set completely invalid value $invalid_speed\n";
    print $cwsocket chr(27).'2'.$invalid_speed;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;

    return;
}
