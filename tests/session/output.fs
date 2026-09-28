\ words that print, checked against the session transcript
72 emit 105 emit cr
3 spaces 33 emit cr
bl .
104 here c!
105 here 1+ c!
here 2 type cr
here 0 type cr
here 65 c, 66 c, 67 c, 3 type cr
: greet s" hello" type cr ;
greet
: banner ." ash forth" cr ;
banner
: nothing s" " type ;
nothing 42 .
: countdown begin dup . 1- dup 0= until drop ;
5 countdown
: idx 5 0 do i . loop ;
idx
\ stack must balance
depth .
