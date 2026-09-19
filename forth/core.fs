\ core.fs -- the Forth layer of Ash.
\ Everything here is derived: if a word can be written in Forth,
\ it lives in this file, not in the C kernel.

\ stack words

: nip   swap drop ;
: tuck  swap over ;
: 2dup  over over ;
: 2drop drop drop ;

\ arithmetic

: negate 0 swap - ;
: 1+ 1 + ;
: 1- 1 - ;
: 2* 2 * ;
: 2/ 2 / ;
