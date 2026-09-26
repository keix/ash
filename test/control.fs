\ control flow written in forth
: even? 2 mod 0= if 111 else 222 then . ;
4 even?
5 even?
-7 abs . 3 abs .
3 9 min . 9 3 min . 3 9 max . 9 3 max .
5 ?dup . . 0 ?dup .
: countdown begin dup . 1- dup 0= until drop ;
5 countdown
\ stack must balance
depth .
