#!/usr/bin/perl


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




# Test script for cwdaemon.
#
# This script tests cwdaemon's responses to <ESC>a escaped
# request. The request is used to turn on or turn off PTT function.
#
# This script works in three modes:
#  - manual mode (0): toggling PTT pin manually purely by sending PTT Escape
#    request (the default mode)
#  - automatic mode (1): toggling PTT pin automatically at the beginning and
#    end of plaing a Caret request.
#  - mixed mode (2): setting PTT ON before a Caret request, and setting PTT
#    OFF after receing reply to the Caret request.




use warnings;
use strict;

use IO::Socket::INET;
use IO::Handle;
use Getopt::Long;



use lib "./";
use cwdaemon::test::common;



my $request_code = 'a';   # Code of Escape request
my $input_text = "paris madrit";     # Text to be played.
my $g_mode = 0; # 0 = manual, 1 = automatic, 2 = mixed


my $result = GetOptions("mode=i"     => \$g_mode)
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



cwdaemon::test::common::esc_set_initial_parameters($cwsocket);
# Re-enable the toggling of PTT by setting non-zero PTT delay.
print $cwsocket chr(27)."d"."1";



# Enough iterations to make good measurements with multimeter. You don't
# really need to observe all 100 iterations :)
my g_n_iters = 100;

if (0 == $g_mode) {
    print "[II] Manual mode\n";
    for (my $i = 0; $i <= $g_n_iters; $i += 1) {
	print "[II] Setting PTT state ON\n";
	print $cwsocket chr(27).$request_code."1";
	sleep 3;

	print "[II] Setting PTT state OFF\n";
	print $cwsocket chr(27).$request_code."0";
	sleep 3;
    }
} elsif (1 == $g_mode) {
    print "[II] Automatic mode: toggling PTT when playing Caret request\n";
    for (my $i = 0; $i <= $g_n_iters; $i += 1) {
	print "[II] Sending Caret request\n";
	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;
	sleep 3;
    }
} elsif (2 == $g_mode) {
    print "[II] Mixed mode: manually toggling PTT before and after playing Caret request\n";
    for (my $i = 0; $i <= $g_n_iters; $i += 1) {

	print "[II] Manually setting PTT state ON\n";
	print $cwsocket chr(27).$request_code."1";
	sleep 3;

	print "[II] Sending Caret request\n";
	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;
	sleep 3;

	print "[II] Manually setting PTT state OFF\n";
	print $cwsocket chr(27).$request_code."0";
	sleep 3;
    }
} else {
    print "[EE] Invalid mode $g_mode\n";
}




$cwsocket->close();

