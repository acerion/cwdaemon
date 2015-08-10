#!/bin/bash


# Test script for cwdaemon.
# 
# This script calls ./cwtest_esc5.pl to check if cwdaemon properly
# exits upon receiving <ESC>5 escaped request.


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




help()
{
    echo "Usage: $0 [-c <int>] [-d <debug level>]" 1>&2;
    exit 1;
}




cycles=5;
debug_level="I"
fork_option="-n"




while getopts ":c:d:" o; do
    case "${o}" in
        c)
            cycles=${OPTARG}
            ;;
        d)
            debug_level=${OPTARG}
            ;;
        *)
            help
            ;;
    esac
done
shift $((OPTIND-1))

	  


term=xterm
font="-misc-fixed-medium-r-normal--18-120-100-100-c-90-iso8859-2"
cwdaemon_dir=$(pwd)
cwdaemon_command="../src/cwdaemon $fork_option --tone 300 -x a -y $debug_level"




echo "cwdaemon command is $cwdaemon_command"




# Delay after exit of cwdaemon. You will see "requested exit cwdaemon"
# message for this many seconds in cwdaemon's terminal window.
post_delay=3;


# echo "We are in $cwdaemon_dir"


for cycle in `seq 1 $cycles`; do

    echo "Cycle $cycle/$cycles"
    
    
    # Main window, where cwdaemon will be run.
    # Don't fork cwdaemon, I want to see it exiting.
    
    # $term -geometry 80x25+0+0 -fn $font -e "/bin/bash --init-file  <(echo \"$cwdaemon_command   &&   exec 0>&-\")   &&   sleep $post_delay" &
    $cwdaemon_command &
    cwdaemon_pid=$!
    
    
    # Wait for start of cwdaemon before sending any requests to it.
    sleep 1

    
    # This script will send a text request followed by "<ESC>5" (quit
    # cwdaemon) request.
    ./cwtest_esc5.pl &
    script_pid=$!


    wait $cwdaemon_pid
    cwdaemon_return=$?
    
    # echo "return code of cwdaemon process $cwdaemon_pid is $cwdaemon_return (script pid is $script_pid)"

    echo ""

    if [ $cwdaemon_return != 0 ]; then
	echo "ERROR: cwdaemon's return code in cycle $cycle/$cycles is $cwdaemon_return"
	exit;
    else
	echo "INFO: cwdaemon's return code in cycle $cycle/$cycles is $cwdaemon_return"
    fi
    
    
    # sleep $(($post_delay + 1));

    echo ""

    
done

