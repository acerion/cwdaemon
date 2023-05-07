#!/usr/bin/perl


# example.pl - example script for cwdaemon
# Copyright (C) 2012 Jenö Vágó  HA5SE
# Copyright (C) 2012 - 2023 Kamil Ignacak
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
STDERR->autoflush(1);
STDOUT->autoflush(1);

# How many times to run a basic set of send/receive calls.
my $cycles = 5;



my $server_port = 6789;

my $cwsocket = IO::Socket::INET->new(PeerAddr => "localhost",
				     PeerPort => $server_port,
				     Proto    => "udp",
				     Type     => SOCK_DGRAM)
    
    or die "Couldn't setup udp server on port $server_port : $@\n";


sub INT_handler {

    $cwsocket->close();

    exit(0);

}

$SIG{'INT'} = 'INT_handler';

my @words = qw/Lorem ipsum dolor sit amet consectetur adipisicing elit sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident sunt in culpa qui officia deserunt mollit anim id est laborum./;






for (my $i = 0; $i < $cycles; $i++) {
    
    # PTT ON
    print "PTT ON\n\n";
    print $cwsocket chr(27).'a1';

    my $word = @words[rand(50)];
    print " sending \"".$word."\"\t";

    # First define reply expected from daemon (in this example it's
    # first two letters of a word). This is Escaped request 'h'.
    print $cwsocket chr(27).'h'.substr($word, 0, 2);

    print $cwsocket $word."";

    # Leading 'h' will be stripped from reply by example_receive().
    my $reply = example_receive($cwsocket, "h");

    if ($reply ne substr($word, 0, 2)) {
	print("\t\tWrong reply\n");
    } else {
	print("\t\tCorrect reply\n");
    }
}




print("\nThe end. Asking cwdaemon to exit...\n");
# Exit cwdaemon (Escaped request '5').
print $cwsocket chr(27).'5';





$cwsocket->close();





sub example_receive
{
    my $cwsocket = shift;
    my $expected_prefix = shift;

    my $expected_postfix = "\r\n";
    my $pre_len = length($expected_prefix);
    my $post_len = length($expected_postfix);


    my $reply = <$cwsocket>;


    if (substr($reply, 0, $pre_len) ne $expected_prefix) {
	print("malformed reply, missing leading '$expected_prefix'");
	return "";
    }


    if (substr($reply, length($reply) - $post_len, $post_len) ne $expected_postfix) {
	print("malformed reply, missing ending '\\r\\n'");
	return "";
    }


    $reply = substr($reply, $pre_len, length($reply) - $pre_len - $post_len);
    print("received ");
    if ($pre_len) {
	print("'" . $expected_prefix . "' + ");
    }
    print("'" . $reply . "' + '\\r\\n'");


    return $reply;
}
