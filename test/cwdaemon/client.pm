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


    my $received_prefix = substr($reply, 0, $pre_len);
    if ($received_prefix ne $expected_prefix) {
	#print("malformed reply, missing prefix '$expected_prefix'");
	return (undef, undef, undef);
    }


    my $received_text = substr($reply, $pre_len, length($reply) - $pre_len - $post_len);


    my $received_postfix = substr($reply, length($reply) - $post_len, $post_len);
    if ($received_postfix ne $expected_postfix) {
	#print("malformed reply, missing postfix '\\r\\n'");
	return  ($received_prefix, $received_text, undef);
    }

   
    # At this point 'reply' may be an empty string.
    return ($received_prefix, $received_text, $received_postfix);
    #return (undef, $received_text, $received_postfix);
}





# This function sends following request to the server:
# "<ESC>h<empty or non-empty reply text>"
# "<single- or multi-character text to be played>"
#
# This function expects the following reply from the server:
# "h<empty or non-empty reply text>\r\n"
sub send_request_esc_h
{
    my $cwsocket            = shift;
    my $request_text        = shift;
    my $reply_expected_text = shift;
    
    # Use "<ESC>h" request to define reply text that server should
    # send to client after playing a text from regular request.
    print $cwsocket chr(27) . "h" . $reply_expected_text;
    # Use regular request to ask server to play a text.
    print $cwsocket $request_text;
}





# This function sends following request to the server:
# "<single- or multi-character text to be played>^"
#
# This function expects the following reply from the server:
# "<single-or multi-character text to be played>\r\n"
sub send_request_caret
{
    my $cwsocket     = shift;
    my $request_text = shift;

    # Use caret request to do two things at once:
    # a. ask server to play given text, and
    # b. define reply text that server should send to client after
    # playing the text
    
    # "^" after the text tells the server to use request text as a
    # reply text.
    print $cwsocket $request_text . '^';
}





1;
