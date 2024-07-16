#!/usr/bin/perl


# cwtest.pl - test script for cwdaemon
# Copyright (C) 2012 Jenö Vágó  HA5SE
# Copyright (C) 2012 - 2024 Kamil Ignacak
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
use Time::HiRes qw(usleep);
use Getopt::Long;

use lib "./";
use cwdaemon::client;





# How many times to run a basic set of tests.
my $cycles = 50000;
my $cycle = 0;

# Number of test strings (number of request/reply pairs) to be used in
# each test.
my $n_test_strings = 20;

# Milliseconds multiplier.
# How long to sleep after every basic set of tests (the set is run in loop) [milliseconds].
my $in_loop_sleep_milliseconds = 1;

# Some test strings are randomly generated, and have random
# length. This is maximal random length.
my $string_len_max = 10;


my $result = GetOptions("cycles=i"       => \$cycles,
			"nstrings=i"     => \$n_test_strings,
			"stringlenmax=i" => \$string_len_max)

    or die "[EE] Problems with getting options: $@\n";




# Give tester some time to a the description of a test before the test is executed.
sub pre_test_delay()
{
    sleep(4);
}




my $seed = time;
print "Using seed $seed.\n\n";
srand($seed);





# Number of tokens (tX) and errors (eX) in cwtest_send_X().
my $global_total = 0;
my $global_errors = 0;
my $global_t0 = 0;
my $global_e0 = 0;
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





my $server_port = 6789;
my $cwsocket = IO::Socket::INET->new(PeerAddr => "localhost",
				     PeerPort => $server_port,
				     Proto    => "udp",
				     Type     => SOCK_DGRAM)
    or die "Couldn't setup udp server on port $server_port : $@\n";





sub INT_handler
{
    print "\n";
    cwtest_print_stats($cycle, $cycles);
    $cwsocket->close();

    exit(0);
}

$SIG{'INT'} = 'INT_handler';





for ($cycle = 0; $cycle < $cycles; $cycle++) {

    &cwdaemon_test0;
    $global_t0 += $global_total;
    $global_e0 += $global_errors;
    # clear before next test
    $global_total = 0;
    $global_errors = 0;

    &cwdaemon_test1;
    $global_t1 += $global_total;
    $global_e1 += $global_errors;
    # clear before next test
    $global_total = 0;
    $global_errors = 0;

    &cwdaemon_test2;
    $global_t2 += $global_total;
    $global_e2 += $global_errors;
    # clear before next test
    $global_total = 0;
    $global_errors = 0;

    &cwdaemon_test3;
    $global_t3 += $global_total;
    $global_e3 += $global_errors;
    # clear before next test
    $global_total = 0;
    $global_errors = 0;

    &cwdaemon_test4;
    $global_t4 += $global_total;
    $global_e4 += $global_errors;
    # clear before next test
    $global_total = 0;
    $global_errors = 0;

    &cwdaemon_test5;
    $global_t5 += $global_total;
    $global_e5 += $global_errors;
    # clear before next test
    $global_total = 0;
    $global_errors = 0;

    cwtest_print_stats($cycle, $cycles);
}


print "\n";
print "[II] End of test\n";
$cwsocket->close();








# ==========================================================================
#                                    subs
# ==========================================================================





sub cwdaemon_test0
{
    print("\n\n\n--- Test 0:\n");
    print("\n");

    print "PTT ON\n\n";
    print $cwsocket chr(27).'a1';
 
    cwtest_send_hang($cwsocket, "paris ");
}





sub cwdaemon_test1
{
    print("\n\n\n--- Test 1:\n");

    print("escaped request: \"<ESC>h<constant, non-empty reply text>\"\n");
    print("request: \"<single-character request text>\"\n");
    print("expected reply: \"h<non-empty reply text>\\r\\n\"\n");
    print("\n");
    pre_test_delay();

    print "PTT ON\n\n";
    print $cwsocket chr(27).'a1';


    # Single-character request texts
    my @request_texts = cwtest_get_random_strings($n_test_strings, 1);
    
    # Constant, non-empty reply texts
    my @reply_expected_texts = ("ack") x $n_test_strings;
    
    cwtest_request(1, $cwsocket, \@request_texts, \@reply_expected_texts);
}





sub cwdaemon_test2
{
    print("\n\n\n--- Test 2:\n");
    
    print("escaped request: \"<ESC>h<empty reply text>\"\n");
    print("request: \"<single-character request text>\"\n");
    print("expected reply: \"h\\r\\n\"\n");
    print("\n");
    pre_test_delay();

    print "PTT ON\n\n";
    print $cwsocket chr(27).'a1';

    # Single-character request texts
    my @request_texts = cwtest_get_random_strings($n_test_strings, 1);
    
    # Empty reply texts.
    my @reply_expected_texts = ("") x $n_test_strings;
    
    cwtest_request(1, $cwsocket, \@request_texts, \@reply_expected_texts);
}





sub cwdaemon_test3
{
    print("\n\n\n--- Test 3:\n");

    print("escaped request: \"<ESC>h<random non-empty reply text>\"\n");
    print("request: \"<random multi-character request text>\"\n");
    print("expected reply: \"h<reply text specified in escaped request>\\r\\n\"\n");
    print("\n");
    pre_test_delay();

    print "PTT ON\n\n";
    print $cwsocket chr(27).'a1';

    # Multi-character request texts
    my @request_texts = cwtest_get_random_strings($n_test_strings, $string_len_max);
    
    # Non-constant, non-empty reply texts.
    my @reply_expected_texts = cwtest_get_random_strings($n_test_strings, $string_len_max);
    
    cwtest_request(1, $cwsocket, \@request_texts, \@reply_expected_texts);
}





sub cwdaemon_test4
{
    print("\n\n\n--- Test 4:\n");
    
    print("request: \"<single-character request text>^\"\n");
    print("expected reply: \"<single-character reply==request text>\\r\\n\"\n");
    print("\n");
    pre_test_delay();

    print "PTT OFF\n\n";
    print $cwsocket chr(27).'a0';


    # Single-character request texts.
    my @request_texts = cwtest_get_random_strings($n_test_strings, 1);
    
    # Single-character reply texts equal to request texts.
    my @reply_expected_texts = @request_texts;
    
    cwtest_request(0, $cwsocket, \@request_texts, \@reply_expected_texts);
}





sub cwdaemon_test5
{
    print("\n\n\n--- Test 5:\n");
    
    print("request: \"<multi-character request text>^\"\n");
    print("expected reply: \"<multi-character reply==request text>\\r\\n\"\n");
    print("\n");
    pre_test_delay();

    print "PTT OFF\n\n";
    print $cwsocket chr(27).'a0';

    # Multi-character request texts
    my @request_texts = cwtest_get_random_strings($n_test_strings, $string_len_max);

    # Multi-character reply texts equal to request texts.
    my @reply_expected_texts = @request_texts;

    cwtest_request(0, $cwsocket, \@request_texts, \@reply_expected_texts);
}





# This function can perform two modes of communication with cwdaemon server.
#
# First mode of communication (Escape mode) first sends two requests:
# One to define reply from server, and one to define text to be played:
# "<ESC>h<empty or non-empty reply text>"
# "<single- or multi-character text to be played>"
#
# Then function waits for reply from server. In the first type of
# communication the reply has following form:
#
# "h<empty or non-empty reply text>\r\n"
#
#
#
# Second mode of communication sends only one request - a request that
# defines text to be played, which is followed by caret character
# ('^'). The caret character means that the text to be played is also
# a text to be sent in reply from server to client.
#
#
#
# Mode of communication is selected with first argument: $esc_mode,
# being 1 for Escape mode, and 0 for caret mode.
sub cwtest_request
{
    my $esc_mode = shift; 
    my $cwsocket = shift;
    my ($request_texts, $reply_expected_texts) = @_;

    my $n_requests = @$request_texts;
    my $n_replies = @$reply_expected_texts;

    if ($n_requests != $n_replies) {
	die "Lengths of args don't match";
    }


    # Constant expected reply text.
    #
    # cwdaemon allows 'expected_reply' to be empty string, this will
    # be tested elsewhere.

    for (my $i = 0; $i < $n_requests; $i++) {

	my $request_prefix = "";
	my $request_text = @$request_texts[$i];
	my $reply_expected_text = @$reply_expected_texts[$i];


	print("\n");
	print("sending request text           '$request_text'\n");
	print("    expecting reply text:      '$reply_expected_text'\n");


	if ($esc_mode == 1) {
	    $request_prefix = "h";
	    cwdaemon::client::send_request_esc_h($cwsocket, $request_text, $reply_expected_text);
	} else {
	    $request_prefix = "";
	    cwdaemon::client::send_request_caret($cwsocket, $request_text);
	}


	my ($reply_prefix, $reply_text, $reply_postfix) = cwdaemon::client::receive($cwsocket, $request_prefix);
	$global_total++;



	if (!defined($reply_prefix) || !defined($reply_text) || !defined($reply_postfix)) {
	    $global_errors++;
	    print("undefined\n");

	} elsif ($reply_prefix ne $request_prefix || $reply_text ne $reply_expected_text || $reply_postfix ne "\r\n") {
	    $global_errors++;
	    print("unequal\n");

	} else {
	    print("    received full reply: ");
	    if (length($reply_prefix) != 0) {
		print("'$reply_prefix' + ");
	    } else {
		print("      ");
	    }
	    print("'$reply_text' + '\\r\\n'");
	}

	print "\n";

	my $r = rand($in_loop_sleep_milliseconds * 1000);
	usleep(int($r));
    }
}





# This function sends following request to the server:
# "<ESC>h<non-empty reply text>"
# "<single-character text to be played>"
#
# This function expects the following reply from the server:
# "h<non-empty reply text>\r\n"
sub cwtest_send_hang
{
    my $cwsocket = shift;
    my $txt = shift;

    for (my $i = 0; $i < length($txt); $i++) {
	# cwdaemon allows 'expected' to be empty string,
	# this will be tested in cwtest_send_2.
	my $expected = "reply";
	my $request_prefix = "h";

	my $text = substr($txt, $i, 1);

	print "sending  '" . $text . "'\n";

        # Use "<ESC>h" request to define reply expected from the server.
	print $cwsocket chr(27) . $request_prefix . $expected;
	# Send text to be played by server.
	print $cwsocket $text;

	my ($reply_prefix, $reply_text, $reply_postfix) = cwdaemon::client::receive($cwsocket, $request_prefix);

	$global_total++;
	if ($reply_text ne $expected) {
	    $global_errors++;
	    die "die 1, incorrect reply: '$reply_text' != '$expected'\n";
	}

	print "\n"; # To clearly separate pairs of send/reply from each other.


	usleep(10000);
    }
}




# Get an array of random strings
# The strings can be used as request texts or expected reply texts.
sub cwtest_get_random_strings
{
    my $n_strings = shift;
    my $len_max = shift;

    my @chars = ("A".."Z", "a".."z", "0".."9");
    my @spaces = (" ") x 20;
    @chars = (@chars, @spaces);

    my @strings = ("") x $n_strings;
    for (my $i = 0; $i < $n_strings; $i++) {

	# http://www.perlmonks.org/?node_id=233023
	my $string;
	my $n = int(rand($len_max));
	$string .= $chars[rand @chars] for 0..$n;

	$strings[$i] = $string;
    }

    return @strings;
}





sub cwtest_print_stats
{
    my $cycle = shift;
    my $cycles = shift;

    printf("\n=================================================\n");
    printf("Finished cycle " . ($cycle + 1) . " / $cycles\n");

    printf("Stats for test 0: $global_e0 errors / $global_t0 tokens (");
    if ($global_t0) {
	printf("%.4f%%", 1.0 * $global_e0 / $global_t0);
    } else {
	printf("%.4f%%", 0.0);
    }
    printf(")\n");

    printf("Stats for test 1: $global_e1 errors / $global_t1 tokens (");
    if ($global_t1) {
	printf("%.4f%%", 1.0 * $global_e1 / $global_t1);
    } else {
	printf("%.4f%%", 0.0);
    }
    printf(")\n");

    printf("Stats for test 2: $global_e2 errors / $global_t2 tokens (");
    if ($global_t2) {
	printf("%.4f%%", 1.0 * $global_e2 / $global_t2);
    } else {
	printf("%.4f%%", 0.0);
    }
    printf(")\n");

    printf("Stats for test 3: $global_e3 errors / $global_t3 tokens (");
    if ($global_t3) {
	printf("%.4f%%", 1.0 * $global_e3 / $global_t3);
    } else {
	printf("%.4f%%", 0.0);
    }
    printf(")\n");

    printf("Stats for test 4: $global_e4 errors / $global_t4 tokens (");
    if ($global_t4) {
	printf("%.4f%%", 1.0 * $global_e4 / $global_t4);
    } else {
	printf("%.4f%%", 0.0);
    }
    printf(")\n");

    printf("Stats for test 5: $global_e5 errors / $global_t5 tokens (");
    if ($global_t5) {
	printf("%.4f%%", 1.0 * $global_e5 / $global_t5);
    } else {
	printf("%.4f%%", 0.0);
    }
    printf(")\n");

    printf("=================================================\n\n");

}

