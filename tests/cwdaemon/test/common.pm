#!/usr/bin/perl


# Module with common utility functions for cwdaemon test scripts.


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





use strict;
use POSIX;

package cwdaemon::test::common;





# Set initial, valid values of some basic parameters.
#
# The function may turn out to be useful when several test scripts are
# run one after another, and each leaves cwdaemon with different
# speed, volume or tone. This function re-sets these values to some
# sane defaults.
#
# For now number of parameters is very limited.
sub esc_set_initial_parameters
{
    my $cwsocket = shift;

    my $initial_volume = 70;     # As in cwdaemon.c
    print "    Setting initial volume $initial_volume\n";
    print $cwsocket chr(27).'g'.$initial_volume;

    my $initial_speed = 35;      # NOT as in cwdaemon.c. Value higher than the default 24 wpm speeds up the tests.
    print "    Setting initial speed $initial_speed\n";
    print $cwsocket chr(27).'2'.$initial_speed;

    my $initial_tone = 400;      # NOT as in cwdaemon.c. More ear-friendly during long tests
    print "    Setting initial tone $initial_tone\n";
    print $cwsocket chr(27).'3'.$initial_tone;

    my $initial_weight = 0;      # As in cwdaemon.c
    print "    Setting initial weight $initial_weight\n";
    print $cwsocket chr(27).'7'.$initial_weight;

    my $initial_delay = 0;      # As in cwdaemon.c
    print "    Setting initial delay $initial_delay\n";
    print $cwsocket chr(27).'d'.$initial_delay;

    return;
}





# Set initial valid value of escaped request
sub esc_set_initial_valid_send
{
    my $cwsocket = shift;
    my $request_code = shift;
    my $input_text = shift;

    my $valid_value = shift;



    # Set a valid initial value of a parameter
    print "    Setting initial valid value $valid_value\n";
    print $cwsocket chr(27).$request_code.$valid_value;

    print $cwsocket $input_text."^";
    my $reply = <$cwsocket>;



    return;
}





# Try setting a single invalid value of escaped request
#
# The subroutine can be used to set some specific, random, invalid
# value of a parameter.
sub esc_set_invalid_send
{
    my $cwsocket = shift;
    my $request_code = shift;
    my $input_text = shift;

    my $invalid_value = shift;



    print "    Trying to set invalid value $invalid_value\n";
    print $cwsocket chr(27).$request_code.$invalid_value;

    print $cwsocket $input_text."^";
    my $reply = <$cwsocket>;



    return;
}





# Try setting invalid, "out of range" values of parameter
#
# cwdaemon reads values of some escaped requests (speed, tone,
# weighting and some other) as "long int". Try to see what happens
# when values sent to cwdaemon exceed limits (lower and upper limit)
# of "long int" type.
sub esc_set_oor_long_send
{
    my $cwsocket = shift;
    my $request_code = shift;
    my $input_text = shift;



    # Try to set the other out of range long value
    my $invalid_value = POSIX::LONG_MIN * 1000000;

    print "    Trying to set negative out of range long value $invalid_value\n";
    print $cwsocket chr(27).$request_code.$invalid_value;

    print $cwsocket $input_text."^";
    my $reply = <$cwsocket>;



    # Try to set out of range long value
    $invalid_value = POSIX::LONG_MAX * 1000000;

    print "    Trying to set positive out of range long value $invalid_value\n";
    print $cwsocket chr(27).$request_code.$invalid_value;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;



    return;
}





# Try setting invalid, float values of integer parameter
#
# cwdaemon reads values of some escaped requests (speed, tone,
# weighting and some other) as "long int". Try to see what happens
# when values sent to cwdaemon are float numbers.
#
# Notice that all sent values have non-zero part after decimal
# point. These will be rejected.
#
# Any string representing float value with all zeros after decimal
# point will be accepted.  TODO: perhaps there should be a test for
# this case.
sub esc_set_invalid_float_send
{
    my $cwsocket = shift;
    my $request_code = shift;
    my $input_text = shift;


    my $n_floats = 15;
    my $invalid_value = 0.01;

    for (my $f = 0; $f < $n_floats; $f++) {

	my $invalid_string = sprintf("%f", $invalid_value);

	print "    Trying to set positive float value $invalid_string\n";
	print $cwsocket chr(27).$request_code.$invalid_string;

	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;

	$invalid_value *= 3.1;
    }


    $invalid_value = -0.01;

    for (my $f = 0; $f < $n_floats; $f++) {

	my $invalid_string = sprintf("%f", $invalid_value);

	print "    Trying to set negative float value $invalid_string\n";
	print $cwsocket chr(27).$request_code.$invalid_string;

	print $cwsocket $input_text."^";
	my $reply = <$cwsocket>;

	$invalid_value *= 3.1;
    }


    return;
}





# Try setting value of escaped request that is empty
sub esc_set_empty_send
{
    my $cwsocket = shift;
    my $request_code = shift;
    my $input_text = shift;



    # Try to set empty parameter value
    my $invalid_value = "";

    print "    Trying to set empty value\n";
    print $cwsocket chr(27).$request_code.$invalid_value;

    print $cwsocket $input_text."^";
    my $reply = <$cwsocket>;



    return;
}





# Try setting value of escaped request that is not a number
#
# For some escaped requests (speed, tone, etc.) NaN is obviously
# invalid.
sub esc_set_nan_send
{
    my $cwsocket = shift;
    my $request_code = shift;
    my $input_text = shift;




    # Try to set a single character
    my $invalid_value = "k";

    print "    Trying to set character NaN value '$invalid_value'\n";
    print $cwsocket chr(27).$request_code.$invalid_value;

    print $cwsocket $input_text."^";
    my $reply = <$cwsocket>;




    # Try to set a string of characters
    my $invalid_value = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";

    print "    Trying to set string NaN value \"$invalid_value\"\n";
    print $cwsocket chr(27).$request_code.$invalid_value;

    print $cwsocket $input_text."^";
    my $reply = <$cwsocket>;



    return;
}





# Try setting value of parameter that is by one lower than lower limit
# (min-1) and that is by one higher than upper limit (max+1).
sub esc_set_min1_max1_send
{
    my $cwsocket = shift;
    my $request_code = shift;
    my $input_text = shift;

    my $valid_min = shift;
    my $valid_max = shift;



    # Then try to set value that is too low
    my $invalid_value = $valid_min - 1;

    print "    Trying to set invalid low value (min-1) $invalid_value\n";
    print $cwsocket chr(27).$request_code.$invalid_value;

    print $cwsocket $input_text."^";
    my $reply = <$cwsocket>;



    # Then try to set value that is too high
    $invalid_value = $valid_max + 1;

    print "    Trying to set invalid high value (max+1) $invalid_value\n";
    print $cwsocket chr(27).$request_code.$invalid_value;

    print $cwsocket $input_text."^";
    $reply = <$cwsocket>;



    return;
}





1;
