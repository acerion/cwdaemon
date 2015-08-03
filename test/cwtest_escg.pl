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


# This script tests cwdaemon's responses to <ESC>g escaped
# request. The request is used to modify volume of sending Morse code
# (%).



use warnings;
use strict;

use IO::Socket::INET;
use IO::Handle;
use Getopt::Long;



use cwdaemon::test::common;



my $volume_min = 0;
my $volume_max = 100;



# How many times to run a basic set of tests.
my $cycles = 5;
my $cycle = 0;


my $input_text = "p";


my $delta = 4;   # Change (in %) per one step in a loop.


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

    print "Testing setting volume in valid range\n";
    &cwdaemon_test0;

    print "\n";

    print "Testing setting invalid values\n";
    &cwdaemon_test1;
}


$cwsocket->close();








# ==========================================================================
#                                    subs
# ==========================================================================





# Testing setting volume in valid range
sub cwdaemon_test0
{
    # Volume going from min to max
    for (my $volume = $volume_min; $volume <= $volume_max; $volume += $delta) {

	print "    Setting volume $volume (up)\n";
	print $cwsocket chr(27).'g'.$volume;

	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;
    }


    # Volume going from max to min
    for (my $volume = $volume_max; $volume >= $volume_min; $volume -= $delta) {

	print "    Setting volume $volume (down)\n";
	print $cwsocket chr(27).'g'.$volume;

	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;
    }

    return;
}





# Testing setting invalid values of <ESC>g request
sub cwdaemon_test1
{
    my $valid_volume = 30;

    # First set a valid volume
    print "    Setting initial valid volume $valid_volume\n";
    print $cwsocket chr(27).'g'.$valid_volume;

    print $cwsocket $input_text."^";
    my $reply = <$cwsocket>;




    # Then try to set volume that is too low
    my $invalid_volume = $volume_min - 1;

    print "    Trying to set invalid low volume $invalid_volume\n";
    print $cwsocket chr(27).'g'.$invalid_volume;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;




    # Then try to set volume that is too low
    $invalid_volume = $volume_max + 1;

    print "    Trying to set invalid high volume $invalid_volume\n";
    print $cwsocket chr(27).'g'.$invalid_volume;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;




    # Try to set totally invalid volume
    $invalid_volume = -1;

    print "    Trying to set negative volume $invalid_volume\n";
    print $cwsocket chr(27).'g'.$invalid_volume;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;




    # Another attempt at totally invalid volume
    $invalid_volume = $volume_max * 100000;

    print "    Trying to set very large volume $invalid_volume\n";
    print $cwsocket chr(27).'g'.$invalid_volume;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;




    # Now something that is not a number
    $invalid_volume = "k";

    print "    Trying to set completely invalid value $invalid_volume\n";
    print $cwsocket chr(27).'g'.$invalid_volume;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;

    return;
}
