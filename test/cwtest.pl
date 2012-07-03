#!/usr/bin/perl


# cwtest.pl - test script for cwdaemon
# Copyright (C) 2012 Jenö Vágó  HA5SE
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


my $server_port = 6789;
my $txt = 'paris ';

my $cwsocket = IO::Socket::INET->new(PeerAddr => "localhost",
		PeerPort => "6789", Proto    => "udp", Type     => SOCK_DGRAM,)
#		PeerPort => "6789", Proto    => "udp")
	or die "Couldn't setup udp server on port $server_port : $@\n";



# print $cwsocket "$txt \n";

#sleep 1;


	print $cwsocket chr(27).'a1';		# PTT ON

for (my $i=0; $i < length($txt); $i++)
{
	print $cwsocket substr($txt, $i, 1);
	print "sending " . substr($txt, $i, 1) . "\n";
	print $cwsocket chr(27) . "hreply";
	my $data = <$cwsocket>;
	printf "data rcvd=$data\n";
#	sleep 1;
}

	print $cwsocket chr(27).'a0';		# PTT OFF

	print $cwsocket ' es ';


for (my $i=0; $i < length($txt); $i++)
{
	print $cwsocket substr($txt, $i, 1) . '^';
	print "sending " . substr($txt, $i, 1) . "\n";
	my $data = <$cwsocket>;
	printf "data rcvd=$data\n";
#	sleep 1;
}
