# cwdaemon.spec
#
# Copyright (c) 2003 -2004 Joop Stakenborg pg4i@amsat.org
#
%define name cwdaemon
%define version 0.9.4
%define release 1

# required items
Name: %{name}
Version: %{version}
Release: %{release}
Copyright: GPL
Group: Daemons
Prefix: /usr
BuildRoot: /var/tmp/%{name}-%{version}
Summary:  Morse daemon for the parallel or serial port
Vendor: Joop Stakenborg <pg4i@amsat.org>
URL: http://www.qsl.net/pg4i/linux/cwdaemon.html
Packager: Joop Stakenborg <pg4i@amsat.org>
Source: %{name}-%{version}.tar.gz

%description
Cwdaemon is a small daemon which uses the pc parallel or serial port 
and a simple transistor switch to output morse code to a transmitter
from a text message sent to it via the udp internet protocol.

%prep
%setup -q

%build
export RPM_OPT_FLAGS="-O2 -march=i386"
export CFLAGS="-O2"
%configure
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc ChangeLog
/usr/sbin/cwdaemon
/usr/share/man/man8/cwdaemon.8.gz
/usr/share/cwdaemon/README
/usr/share/cwdaemon/parallelport_circuit.ps
/usr/share/cwdaemon/serialport_circuit.ps
/usr/share/cwdaemon/parallelport_circuit.jpg
/usr/share/cwdaemon/serialport_circuit.jpg
/usr/share/cwdaemon/cwtest.sh
/usr/share/cwdaemon/cwsetup.sh
/usr/share/cwdaemon/cwtest.c
/usr/share/cwdaemon/cwdaemon.png

%changelog
* Sat Jan 18 2003 Joop Stakenborg
- Initial spec file
