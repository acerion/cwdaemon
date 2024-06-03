# On tty devices

## DTR and RTS lines are set to '1' when a tty device is being open

[\[PATCH v2 0/7\] tty: add flag to suppress ready signalling on open](https://lore.kernel.org/linux-serial/X8iuCXYhOBVMGvXv@localhost/T/)

[Revisiting unwanted auto-assertion of DTR & RTS on serial port open](https://www.spinics.net/lists/linux-serial/msg48224.html)

Looks like the problem has been addressed on FreeBSD, but not on Linux.

Other links on this subject:

<https://unix.stackexchange.com/questions/446088/how-to-prevent-dtr-on-open-for-cdc-acm>

<https://github.com/npat-efault/picocom/blob/master/lowerrts.md>

<https://github.com/Fazecast/jSerialComm/issues/437>

<https://stackoverflow.com/questions/5090451/how-to-open-serial-port-in-linux-without-changing-any-pin>


## Misc articles

[Enhanced USB-to-Serial PTT Interface](https://radio.g4hsk.co.uk/2020/03/01/enhanced-usb-to-serial-ptt-interface/)

