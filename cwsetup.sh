#!/bin/sh

PATH="/bin:/sbin:/usr/bin:/usr/bin:/usr/local/bin:/usr/local/sbin:."

# check if we are root
if [ "$(id -u)" != "0" ]; then
	echo "You must run this script as root..."
	exit 1
fi

job=0

echo "Checking what needs to be done"

# check for parport devices
if [ ! -c /dev/parport0 ]; then
	echo "Creating parport devices"
	DIR=`pwd`
	cd /dev
	MAKEDEV parport
	cd $DIR
	job=1
fi

# check for kernel modules
if ! lsmod|grep parport > /dev/null; then
	echo "Loading parport modules"
	modprobe parport_pc
	modprobe parport
	job=1
fi

if lsmod|grep lp|grep " 0 " > /dev/null; then
	echo "Unloading lp module"
	rmmod lp
	job=1
fi

if ! lsmod|grep ppdev > /dev/null; then
	echo "Loading ppdev module"
	modprobe ppdev
	job=1
fi

if [ $job -eq 1 ]; then
	echo "Setup is ready"
else
	echo "Nothing to do, your setup is okay"
fi

echo "Please run \"cwdaemon -n\" (as root) for diagnosis"
