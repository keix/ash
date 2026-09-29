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

\ defining words. does> compiles (does>), which patches the child's
\ code field to dodoes and hands it the thread that follows.

: does> ['] (does>) , ; immediate
: variable create 0 , ;
: constant create , does> @ ;

\ the cell size, measured by forth itself: comma once, see how far
\ here moved. costs one dead cell of data space.

here 0 , here swap - constant cell

: cells cell * ;
: cell+ cell + ;
: >body cell+ ;
: char+ 1+ ;
: chars ;
: aligned cell 1- + cell 1- invert and ;

\ memory words

: -rot rot rot ;
: +! dup @ rot + swap ! ;
: 2! swap over ! cell+ ! ;
: 2@ dup cell+ @ swap @ ;
: 2swap rot >r rot r> ;
: 2over >r >r 2dup r> r> 2swap ;
: fill >r begin dup 0> while over r@ swap c! swap 1+ swap 1- repeat
  2drop r> drop ;

\ character i/o over emit. type uses begin/while, not do/loop:
\ loop runs its body at least once and a string may be empty.

32 constant bl

: cr 10 emit ;
: space bl emit ;
: spaces begin dup 0> while space 1- repeat drop ;
: type begin dup 0> while swap dup c@ emit 1+ swap 1- repeat 2drop ;

\ parsing and strings. ,string copies "-delimited text into the
\ dictionary as a length cell plus bytes -- the shape (s") executes.

: literal ['] lit , , ; immediate
: char parse-name drop c@ ;
: [char] char ['] lit , , ; immediate
: ( 41 parse 2drop ; immediate
: count dup 1+ swap c@ ;

: ,string 34 parse dup ,
  begin dup 0> while swap dup c@ c, 1+ swap 1- repeat 2drop align ;
: s" ['] (s") , ,string ; immediate
: ." ['] (s") , ,string ['] type , ; immediate

\ more arithmetic

: /mod 2dup mod -rot / ;
: u< 2dup xor 0< if nip 0< else - 0< then ;
: s>d dup 0< ;

\ copying memory. move picks the direction, so overlap is safe.

: cmove
  begin dup 0> while
    >r over c@ over c! 1+ swap 1+ swap r> 1-
  repeat drop 2drop ;
: cmove>
  begin dup 0> while
    1- >r over r@ + c@ over r@ + c! r>
  repeat drop 2drop ;
: move >r 2dup u< if r> cmove> else r> cmove then ;

\ compiler surface. latest-xt reads the dictionary layout from
\ forth: link | flags name_len name pad | does | code.

: [ 0 state ! ; immediate
: ] -1 state ! ;
: latest-xt latest cell+ 1+ dup c@ swap 1+ + aligned cell+ ;
: recurse latest-xt , ; immediate
