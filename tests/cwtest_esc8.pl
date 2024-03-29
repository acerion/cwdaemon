#!/usr/bin/perl


# Test script for cwdaemon.
#
# This script tests cwdaemon's responses to <ESC>8 escaped
# request. The request is used to change keying device used by
# cwdaemon.


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



my $request_code = '8';       # Code of Escape request


my $cycles = 5;               # How many times to run a basic set of tests.
my $cycle = 0;
my $input_text = "p";         # Text to be played

# Mix of devices:
# - non-existent devices,
# - non-tty devices,
# - devices not accessible to user (wrong permissions),
# - valid tty devices with proper permissions.
#
# ttyS0 should be always present on test machine. Make sure that user with
# which cwdaemon is started has permissions to access the device.
#
# ttyUSB0 may or may not be present, depending on whether you plugged
# USB-to-serial converter to USB socket.
#
# cuau0 exists on FreeBSD
#
# TODO: what about parallel-port devices?
my $devices = "ttyS0,null,/dev/ttyUSB0,/dev/tty7,/tmp,cuau0,ttyUSB0,/dev/cuau0,/dev/nonexistent,/dev/ttyS0";


my $result = GetOptions("cycles=i"       => \$cycles,
			"input-text=s"   => \$input_text,
			"devices=s"      => \$devices)

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


    print "Testing switching keying devices\n";
    cwdaemon::test::common::esc_set_initial_parameters($cwsocket);
    &cwdaemon_test;

    print "\n";
}


$cwsocket->close();








# ==========================================================================
#                                    subs
# ==========================================================================





# Testing setting keying devices
sub cwdaemon_test
{
    foreach my $d (split(/,/, $devices)) {

	print "    Setting device \"$d\"\n";
	print $cwsocket chr(27).$request_code.$d;

	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;

	sleep(1);
    }

    return;
}
