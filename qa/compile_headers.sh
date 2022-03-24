#!/bin/bash

root_dir=`pwd`/../
source_dir=$root_dir/src/

includes="-I/usr/include -I$source_dir -I$root_dir"

files=`find $source_dir -type f -name \*.h`
for file in $files;
do
    f=`realpath $file`
    echo "Compiling $f"
    gcc -c -Wall -Wextra -pedantic -std=c99 $includes $f
    rm $file.gch
done

