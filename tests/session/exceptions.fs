\ uncaught throws recover the repl
: boom 99 throw ;
boom
1 2 + .
: bmsg 1 abort" it broke" ;
bmsg
3 4 + .
abort
5 6 + .
1 2 quit 99 .
. .
\ stack must balance
depth .
