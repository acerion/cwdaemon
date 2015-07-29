#!/bin/sh

old_tty_settings=$(stty -g)
stty -icanon
trap 'stty "$old_tty_settings"; exit 0' INT

echo "Press (CTRL-C) to interrupt..."

while true; do
  nc -u localhost 6789
done

