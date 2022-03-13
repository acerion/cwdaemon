A test that verifies that a problem from
https://github.com/acerion/cwdaemon/issues/6 ("cwdaemon stops working after
esc 0 (reset) is issued") doesn't occur anymore.


1. Manually start cwdaemon in the background.
2. In test program send request to cwdaemon to sound a short text.
3. Let cwdaemon sound the text and manipulate DTR pin of serial line port (ttyS0).
4. In test program observe (through "key source serial") the DTR pin, inform
   a receiver about changes to the pin.
5. In test program let the receiver interpret changes of the pin, confirm
   that a received text is the same as requested text.
6. In test program send "esc 0" (reset) to cwdaemon.
7. In test program send another request to cwdaemon to sound another short text.
8. In test program again receive the text.

If a reset request "broke" the cwdaemon, the cwdaemon won't be able to
correctly manipulate DTR pin, and receiver won't receive anything.

If a reset request was correctly handled in cwdaemon, and cwdaemon correctly
re-registered a callback, then cwdaemon will be able to correctly manipulate
DTR pin, which will be observed by key source and then forwarded to receiver.

