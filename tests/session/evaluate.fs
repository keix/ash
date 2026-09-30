\ uncaught throw inside evaluate unwinds the source stack cleanly
: evb s" 9 throw" evaluate ;
evb
1 2 + .
: evx s" no-such" evaluate ;
evx
3 4 + .
\ stack must balance
depth .
