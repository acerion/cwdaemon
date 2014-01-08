#!/usr/bin/perl


# cwtest.pl - test script for cwdaemon
# Copyright (C) 2012 Jenö Vágó  HA5SE
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


use warnings;
use strict;

use IO::Socket::INET;


# How many times to run a basic set of tests.
my $cycles = 50000;
my $cycle = 0; # Iterator.

# How long to sleep after every basic set of tests (the set is run in loop).
my $in_loop_sleep = 2;




my $server_port = 6789;
my $input_text = 'paris ';
#my $input_text = "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

my $cwsocket = IO::Socket::INET->new(PeerAddr => "localhost",
				     PeerPort => $server_port,
				     Proto    => "udp",
				     Type     => SOCK_DGRAM)

    or die "Couldn't setup udp server on port $server_port : $@\n";



# Number of tokens (tX) and errors (eX) in cwtest_send_X().
my $global_t1 = 0;
my $global_e1 = 0;
my $global_t2 = 0;
my $global_e2 = 0;
my $global_t3 = 0;
my $global_e3 = 0;
my $global_t4 = 0;
my $global_e4 = 0;
my $global_t5 = 0;
my $global_e5 = 0;


sub INT_handler {

    print "\n";
    cwtest_print_stats($cycle, $cycles);
    $cwsocket->close();

    exit(0);

}

$SIG{'INT'} = 'INT_handler';





for ($cycle = 0; $cycle < $cycles; $cycle++) {

    print("\n--- Test 1: \"<ESC>h<non-empty reply text>\\r\\n\" ---\n");

    # PTT ON
    print "PTT ON\n\n";
    print $cwsocket chr(27).'a1';

    cwtest_send_1($cwsocket, $input_text);



    print("\n--- Test 2: \"<ESC>h<empty reply text>\\r\\n\" ---\n");
    # PTT still ON
    cwtest_send_2($cwsocket, $input_text);



    print("\n--- Test 3: \"<ESC>h<non-empty reply text>\\r\\n\" ---\n");
    # PTT still ON
    cwtest_send_3($cwsocket, $input_text);



    print("\n--- Test 4: \"<single-character text to be played>^\" ---\n");
    # PTT OFF
    print "PTT OFF\n\n";
    print $cwsocket chr(27).'a0';

    cwtest_send_4($cwsocket, $input_text);



    print("\n--- Test 5: \"<multi-character text to be played>^\" ---\n");
    # PTT still OFF
    cwtest_send_5($cwsocket, $input_text);



    cwtest_print_stats($cycle, $cycles);
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
sub cwtest_send_1
{
    my $cwsocket = shift;
    my $txt = shift;

    for (my $i = 0; $i < length($txt); $i++) {
	# cwdaemon allows 'expected' to be empty string,
	# this will be tested in cwtest_send_2.
	my $expected = "reply";

	my $text = substr($txt, $i, 1);

	print "sending  '" . $text . "'\n";

        # Use "<ESC>h" request to define reply expected from the server.
	print $cwsocket chr(27) . "h" . $expected;
	# Send text to be played by server.
	print $cwsocket $text;

	# Leading 'h' will be stripped from reply by cwtest_receive().
	my $reply = cwtest_receive($cwsocket, "h");

	$global_t1++;
	if ($reply ne $expected) {
	    $global_e1++;
	    die "die 1, incorrect reply: '$reply' != '$expected'\n";
	}

	print "\n"; # To clearly separate pairs of send/reply from each other.

	sleep $in_loop_sleep;
    }
}





# This function sends following to the server:
# "<ESC>h<empty reply text>"
# "<single-character text to be played>"
#
# This function expects the following reply from the server:
# "h\r\n"
sub cwtest_send_2
{
    my $cwsocket = shift;
    my $txt = shift;

    for (my $i = 0; $i < length($txt); $i++) {
	# cwdaemon allows 'expected' to be empty string,
	# so let's test this here.
	my $expected = "";

	my $text = substr($txt, $i, 1);

	print "sending  '" . $text . "'\n";

	# Use "<ESC>h" request to define reply expected from the server.
	print $cwsocket chr(27) . "h" . $expected;
	# Send text to be played by server.
	print $cwsocket $text;

	# Leading 'h' will be stripped from reply by cwtest_receive().
	my $reply = cwtest_receive($cwsocket, "h");

	$global_t2++;
	if ($reply ne $expected) {
	    $global_e2++;
	    die "die 2, incorrect reply: '$reply' != '$expected'\n";
	}

	print "\n"; # To clearly separate pairs of send/reply from each other.

	sleep $in_loop_sleep;
    }
}





# This function sends following requests to the server:
# "<ESC>h<non-empty reply text>"
# "<multi-character text to be played>"
#
# This function expects the following reply from the server:
# "h<non-empty reply text>\r\n"
sub cwtest_send_3
{
    my $cwsocket = shift;
    my $txt = shift;

    my @tokens = split(/ /, $txt);

    foreach (@tokens) {

	my $expected = "ack";
	my $text = $_;

	print "sending  '" . $text . "'\n";

	# Use "<ESC>h" request to define reply expected from the server.
	print $cwsocket chr(27) . "h" . $expected;
	# Send text to be played by server.
	print $cwsocket $text;

	# Leading 'h' will be stripped from reply by cwtest_receive().
	my $reply = cwtest_receive($cwsocket, "h");

	$global_t3++;
	if ($reply ne $expected) {
	    $global_e3++;
	    die "die 3, incorrect reply: '$reply' != '$expected'\n";;
	}


	print "\n"; # To clearly separate pairs of send/reply from each other.

	sleep $in_loop_sleep;
    }
}





# This function sends following to the server:
# "<single-character text to be played>^"
#
# This function expects the following reply from the server:
# "<single-character text to be played>\r\n"
sub cwtest_send_4
{
    my $cwsocket = shift;
    my $txt = shift;

    for (my $i = 0; $i < length($txt); $i++) {

	my $text = substr($txt, $i, 1);

	print "sending  '" . $text . "'\n";

	# "^" after the text tells the server to use text from the request
	# as a text of reply.
	print $cwsocket  $text . '^';

	my $reply = cwtest_receive($cwsocket, "");

	$global_t4++;
	if ($reply ne $text) {
	    $global_e4++;
	    die "die 4, incorrect text in reply: '$reply' != '$text'\n";
	}


	print "\n"; # To clearly separate pairs of send/reply from each other.

	sleep $in_loop_sleep;
    }
}





# This function sends following request to the server:
# "<multi-character text to be played>^"
#
# This function expects the following reply from the server:
# "<multi-character text to be played>\r\n"
sub cwtest_send_5
{
    my $cwsocket = shift;
    my $txt = shift;

    my @tokens = split(/ /, $txt);

    foreach (@tokens) {

	my $text = $_;

	print "sending  '" . $text . "'\n";

	# "^" after the text tells the server to use text from the request
	# as a text of reply.
	print $cwsocket  $text . '^';

	my $reply = cwtest_receive($cwsocket, "");

	$global_t5++;
	if ($reply ne $text) {
	    $global_e5++;
	    die "die 5, incorrect text in reply: '$reply' != '$text'\n";
	}


	print "\n"; # To clearly separate pairs of send/reply from each other.

	sleep $in_loop_sleep;
    }
}





# Also used as receive routine in cwtest_send_2.
sub cwtest_receive
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
    print("'" . $reply . "' + '\\r\\n'\n");


    # At this point 'reply' may be an empty string.
    return $reply;
}





sub cwtest_print_stats
{
    my $cycle = shift;
    my $cycles = shift;

    printf("\n=================================================\n");
    printf("Finished cycle " . ($cycle + 1) . " / $cycles\n");

    printf("Stats 1: $global_e1 errors / $global_t1 tokens (");
    if ($global_t1) {
	printf("%.4f%%", 1.0 * $global_e1 / $global_t1);
    } else {
	printf("%.4f%%", 0.0);
    }
    printf(")\n");

    printf("Stats 2: $global_e2 errors / $global_t2 tokens (");
    if ($global_t2) {
	printf("%.4f%%", 1.0 * $global_e2 / $global_t2);
    } else {
	printf("%.4f%%", 0.0);
    }
    printf(")\n");

    printf("Stats 3: $global_e3 errors / $global_t3 tokens (");
    if ($global_t3) {
	printf("%.4f%%", 1.0 * $global_e3 / $global_t3);
    } else {
	printf("%.4f%%", 0.0);
    }
    printf(")\n");

    printf("Stats 4: $global_e4 errors / $global_t4 tokens (");
    if ($global_t4) {
	printf("%.4f%%", 1.0 * $global_e4 / $global_t4);
    } else {
	printf("%.4f%%", 0.0);
    }
    printf(")\n");

    printf("Stats 5: $global_e5 errors / $global_t5 tokens (");
    if ($global_t5) {
	printf("%.4f%%", 1.0 * $global_e5 / $global_t5);
    } else {
	printf("%.4f%%", 0.0);
    }
    printf(")\n");

    printf("=================================================\n\n");

}

