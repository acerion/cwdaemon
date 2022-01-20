#!/usr/bin/perl


# Test script for cwdaemon.
# 
# This script checks if cwdaemon properly exits upon receiving <ESC>5
# escaped request.  Don't call this script directly, use
# ./cwtest_esc5.sh script to set up and run the test.


# Copyright (C) 2015 - 2022 Kamil Ignacak
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




my $request_code = '5';   # Code of Escape request
my $input_text = "paris";



my $result = GetOptions("input-text=s" => \$input_text)

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




print $cwsocket $input_text."^";
my $reply = <$cwsocket>;
sleep(1);




# Exit cwdaemon.
print $cwsocket chr(27).$request_code;




$cwsocket->close();


