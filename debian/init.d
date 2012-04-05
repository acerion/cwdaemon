#! /bin/sh
### BEGIN INIT INFO
# Provides:          cwdaemon
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:
# Short-Description: A Morse daemon.
### END INIT INFO
#
# Author of this script: Ladislav Vaiz <ok1zia@nagano.cz>
#
# Version:  @(#)cwdaemon  1.0  03-Jul-2004  ok1zia@nagano.cz
#

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/sbin/cwdaemon
NAME=cwdaemon
DESC="Morse daemon"

PIDFILE=/var/run/$NAME.pid
SCRIPTNAME=/etc/init.d/$NAME
OPTS=""

# Gracefully exit if the package has been removed.
test -x $DAEMON || exit 0
# Exit if it says no start
grep -q '^START_CWDAEMON=yes' /etc/default/$NAME || exit 0

# Read config file if it is present.
if [ -r /etc/default/$NAME ]
then
    . /etc/default/$NAME

    test -n "$DEVICE"   && OPTS="$OPTS -d $DEVICE"  
    test -n "$UDPPORT"  && OPTS="$OPTS -p $UDPPORT"  
    test -n "$PRIORITY" && OPTS="$OPTS -P $PRIORITY"  
    test -n "$SPEED"    && OPTS="$OPTS -s $SPEED"  
    test -n "$PTTDELAY" && OPTS="$OPTS -t $PTTDELAY"  
    test -n "$VOLUME"   && OPTS="$OPTS -v $VOLUME"  
    test -n "$WEIGHT"   && OPTS="$OPTS -w $WEIGHT"  
    test -n "$SDEVICE"  && OPTS="$OPTS -x $SDEVICE"  
fi

case "$1" in
  start)
    echo "$DEVICE" | grep -q 'parport'
    if [ $? = 0 ]; then
        lsmod|grep -q '\<lp\>' 
        if [ $? = 0 ]; then
            echo "Removing module lp"
            rmmod lp
        fi
    fi
    
    echo -n "Starting $DESC: $NAME"
    start-stop-daemon --start --quiet --exec $DAEMON -- $OPTS
    echo "."
    ;;
  stop)
    echo -n "Stopping $DESC: $NAME"
    start-stop-daemon --stop --quiet --exec $DAEMON
    echo "."
    ;;
  restart|force-reload)
    echo -n "Restarting $DESC: $NAME"
    start-stop-daemon --stop --quiet --exec $DAEMON
    sleep 1
    start-stop-daemon --start --quiet --exec $DAEMON -- $OPTS
    echo "."
    ;;
  status)
    /bin/ps xa|grep "$DAEMON" | grep -v "grep"
    ;;
  *)
    echo "Usage: $SCRIPTNAME {start|stop|restart|force-reload|status}" >&2
    exit 1
    ;;
esac

exit 0
