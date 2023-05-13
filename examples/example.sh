#!/bin/sh

old_tty_settings=$(stty -g)
stty -icanon
trap 'stty "$old_tty_settings"; exit 0' INT

echo "Press (CTRL-C) to interrupt..."

# On Alpine Linux 1.17 running nc with 'localhost' name doesn't work.
# 127.0.0.1 address must be used instead.
while true; do
  nc -u localhost 6789
done

