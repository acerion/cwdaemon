#!/bin/bash

# Run clang-tidy on file or directory.
# Call this script from project's main directory.
# Pass to it a single argument: path to dir or file that you want to test.

if [ "$#" -ne 1 ]; then
    echo "wrong args"
    exit
fi

path_to_check=$1
root=`pwd`

echo "path_to_check = "$path_to_check
echo "root          = "$root
sleep 2

run-clang-tidy-14 -p=$root $path_to_check


