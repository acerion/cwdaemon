#!/usr/bin/perl


# Module with common utility functions for cwdaemon test scripts.


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





use strict;
package cwdaemon::test::common;





# Set initial, valid values of some basic parameters.
#
# The function may turn out to be useful when several test scripts are
# run one after another, and each leaves cwdaemon with different
# speed, volume or tone. This function re-sets these values to some
# sane defaults.
#
# For now number of parameters is very limited.
sub set_initial_parameters
{
    my $cwsocket = shift;

    my $initial_volume = 70;     # As in cwdaemon.c
    print "    Setting initial volume $initial_volume\n";
    print $cwsocket chr(27).'g'.$initial_volume;

    my $initial_speed = 35;      # NOT as in cwdaemon.c
    print "    Setting initial speed $initial_speed\n";
    print $cwsocket chr(27).'2'.$initial_speed;

    my $initial_tone = 800;      # As in cwdaemon.c
    print "    Setting initial tone $initial_tone\n";
    print $cwsocket chr(27).'3'.$initial_tone;

    return;
}




1;
