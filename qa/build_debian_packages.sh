#!/bin/bash




# Copyright (C) 2023  Kamil Ignacak (acerion@wp.pl)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program. If not, see <https://www.gnu.org/licenses/>.




# Script that builds Debian packages from local copy of project's git repo.
#
# The git repo should contain 'debian' directory with proper contents. If
# there is no such directory, the script will fail. You can modify the script
# to use 'debian' directory or 'debian' archive with such directory (e.g.
# $PACKAGE\_$VERSION.debian.tar.gz) that is pulled by the script from outside
# of the git repo.
#
# Call the script without any arguments.
#
# Call the script either from repo's top-level directory, or from 'qa'
# subdirectory - the script can handle both situations.
#
# The Debian packages are being built outside of the git repo dir. You will
# be prompted by this script to confirm the path in which to build the
# packages.
#
#
# Customize this script by assigning correct values to PACKAGE and VERSION
# variables.




# Customize these variables as needed.
PACKAGE="cwdaemon"
VERSION="0.13.0"




# Project's distribution archive, created by 'make dist'. Should not contain
# 'debian' directory.
dist_archive=$PACKAGE-$VERSION.tar.gz

# Archive with 'debian' dir, as seen in package's page on debian.org.
archive_debian=$PACKAGE\_$VERSION.debian.tar.gz

# 'orig' source code achrive, as seen in package's page on debian.org.
# Generated from packages's distribution archive.
archive_orig=$PACKAGE\_$VERSION.orig.tar.gz

# https://lintian.debian.org/tags/bad-distribution-in-changes-file.html
# https://bugs.launchpad.net/ubuntu/+source/lintian/+bug/1303603
# https://manpages.debian.org/jessie/devscripts/debuild.1.en.html
# /usr/share/lintian/profiles
VENDOR="debian"
LINTIAN_OPTS="--lintian-opts --profile $VENDOR"

debuild_command='debuild -us -uc $LINTIAN_OPTS'




function header()
{
	sleep_time=$1
	text=$2

	echo ""
	echo ""
	echo "*************" $text
	sleep $sleep_time
}




function run_debuild()
{
	build_name=$1

	echo ""
	sleep 1;
	if ! eval $debuild_command ; then
		echo -e "\n"
		echo $build_name "debuild execution has failed"
		exit 1
	else
		echo -e "\n"
		echo $build_name "debuild execution has succeeded"
	fi
}




header 1 "Debian package build script for $PACKAGE-$VERSION"




# Start from top-level in repo, regardless if script is called from qa/ dir
# or from top-level dir in repo.
header 1 "Going to initial location"
me=$(basename "$0")
if [ -f $me ]; then
	cd ..
elif [ -f "qa"/$me ]; then
	: # noop
else
	echo "This script is called from unexpected location"
	exit 1
fi
echo "Initial location is `pwd`"




header 1 "Obtaining location of repo directory"
repo_dir=`pwd`
if [ -z "$repo_dir"  ]; then
    echo "'repo dir' string is empty, exiting"
    exit 1
elif [ "$repo_dir" == "/" ]; then
    echo "repo dir is root directory, exiting"
    exit 1
fi




# TODO (acerion) 2023.08.13: perhaps the directory should be created after
# the path is accepted by developer in next step ("Confirming directories").
header 1 "Establishing Debian build directory location"
rel_build_dir=../deb_build_dir
if [ ! -d $rel_build_dir ]; then
	mkdir $rel_build_dir
fi
cd $rel_build_dir
deb_build_dir=`pwd`
cd - > /dev/null

if [ ! -d $deb_build_dir ]; then
	echo "Build dir for Debian package can't be accessed"
    exit 1
fi




header 1 "Confirming directories"
echo "repo dir         = " $repo_dir
echo "Debian build dir = " $deb_build_dir

# "-n 1" - no need to confirm a character with Enter
read -r -n 1 -p "Directories are OK? [y/N] " response
echo
case $response in
    [yY])
        echo "OK"
        ;;
    *)
        echo "Aborting build"
        exit 1
        ;;
esac




# Clean up old Debian build artifacts
header 1 "Cleaning up Debian build directory"
rm -rf $deb_build_dir/*




# The basis of building the Debian packages will be the distribution archive
# available on sf.net (and Debian's own 'debian' directory). Prepare the
# distribution archive here.
header 1 "Running 'make dist' in repository"
cd $repo_dir

if [ ! -f Makefile ]; then
    ./configure
fi

# 'make' returns zero on success
if ! (make dist) ; then
    echo "Failed to build 'make dist' target, exiting"
    echo `pwd`
    exit 1
fi


if [ -z "$dist_archive"  ]; then
    echo "Failed to find distribution archive, exiting"
    exit 1
fi
echo ""
echo "Distribution archive = $dist_archive"




header 1 "Moving distribution archive to Debian build directory"
mv $dist_archive $deb_build_dir/$archive_orig





# 'debian' directory is not a part of regular distribution package, it needs
# to be moved to deb_build_dir manually.
header 1 "Archiving 'debian' directory in Debian build directory"
# "-C DIR": change to directory DIR.
# Without it the archive would contain full dir hierarchy starting with root dir.

tar cvfz $deb_build_dir/$archive_debian -C $repo_dir debian




# At this point there should be two archives in $deb_build_dir:
# $archive_orig     = $PACKAGE_$VERSION.orig.tar.gz
# $archive_debian   = $PACKAGE_$VERSION.debian.tar.gz
#
# Their structure should match the structure of source code archives
# available at packages.debian.org.
#
# Before we can build a final Debian package, we need to unpack the archives.




header 1 "Going to Debian build dir"
cd $deb_build_dir




header 1 "Unpacking archives in Debian build dir"
deb_final_subdir=$PACKAGE-$VERSION
tar xvf $archive_orig
tar xvf $archive_debian -C $deb_final_subdir
# Don't remove the archives themselves. debuild seems to need them (at least the 'orig' one).




header 1 "Entering final build subdirectory"
echo "Final subdirectory is $deb_final_subdir"
cd $deb_final_subdir




header 1 "Starting first build"
run_debuild "First"




# "-n 1" - no need to confirm a character with Enter
echo ""
echo ""
read -r -n 1 -p "Test doing a second build in build directory? [Y/n] " response
echo
case $response in
	[yY]|"")
		echo "OK"
		run_debuild "Second"
        ;;
    [nN]|*)
		header 1 "Not executing second build"
        # pass
        ;;
esac


