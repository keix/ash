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

\ comparison

: <> = 0= ;
: <= > 0= ;
: >= < 0= ;
: 0< 0 < ;
: 0> 0 > ;

\ control flow -- immediate words over branch / 0branch.
\ if compiles a 0branch with a hole; then patches the hole with here.
\ The hole's address rides the data stack between them, at compile time.

: if    ['] 0branch , here 0 , ; immediate
: then  here swap ! ; immediate
: else  ['] branch , here 0 , swap here swap ! ; immediate

: begin here ; immediate
: until ['] 0branch , , ; immediate
: again ['] branch , , ; immediate

: while  ['] 0branch , here 0 , swap ; immediate
: repeat ['] branch , , here swap ! ; immediate

\ Counted loops compile everything inline, so the loop parameters sit
\ directly on the return stack -- limit below, index on top -- with no
\ helper word's return address between them. i and unloop compile
\ inline for the same reason.

: do   ['] swap , ['] >r , ['] >r , here ; immediate
: loop
  ['] r> , ['] 1+ , ['] r> , ['] 2dup , ['] = ,
  ['] 0branch , here 0 ,
  ['] drop , ['] drop ,
  ['] branch , here 0 ,
  swap here swap !
  ['] >r , ['] >r , ['] branch , swap ,
  here swap ! ; immediate

: i      ['] r@ , ; immediate
: unloop ['] r> , ['] drop , ['] r> , ['] drop , ; immediate

\ derived from control flow

: abs  dup 0< if negate then ;
: min  2dup > if swap then drop ;
: max  2dup < if swap then drop ;
: ?dup dup if dup then ;
