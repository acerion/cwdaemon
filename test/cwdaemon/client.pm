#!/usr/bin/perl


# cwdaemon_client.pl - module with utility functions for cwdaemon
# client applications
#
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





use strict;
package cwdaemon::client;





sub receive
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


    # At this point 'reply' may be an empty string.
    return $reply;
}


1;
