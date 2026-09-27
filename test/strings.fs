\ strings and parsing
: greet s" hello" type cr ;
greet
: banner ." ash forth" cr ;
banner
: nothing s" " type ;
nothing 42 .
( comments work ) 1 2 + .
char A .
: q [char] Q emit cr ;
q
here 3 c, 65 c, 66 c, 67 c, count type cr
align here cell mod .
\ stack must balance
depth .
