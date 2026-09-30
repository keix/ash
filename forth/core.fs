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
: 0<> 0= 0= ;
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

\ counted loops, now that variable exists. Everything compiles
\ inline, so the parameters sit bare on the return stack -- limit
\ below, index on top. leave branches forward before the loop's end
\ is known: each leave hole holds the address of the previous one,
\ and loop walks the chain and patches them all to here.

variable leave-link
0 leave-link !

: (resolve-leaves)
  leave-link @
  begin ?dup while dup @ >r here swap ! r> repeat
  leave-link ! ;

: do  leave-link @ 0 leave-link !
      ['] swap , ['] >r , ['] >r , here ; immediate

: ?do leave-link @ 0 leave-link !
      ['] 2dup , ['] = ,
      ['] 0branch , here 0 ,
      ['] drop , ['] drop ,
      ['] branch , here leave-link @ , leave-link !
      here swap !
      ['] swap , ['] >r , ['] >r , here ; immediate

: loop
  ['] r> , ['] 1+ , ['] r> , ['] 2dup , ['] = ,
  ['] 0branch , here 0 ,
  ['] drop , ['] drop ,
  ['] branch , here 0 ,
  swap here swap !
  ['] >r , ['] >r , ['] branch , swap ,
  here swap !
  (resolve-leaves) ; immediate

\ +loop terminates when the index crosses the limit boundary:
\ sign of (i - limit) differs from sign of (i' - limit).

: +loop
  ['] r> , ['] swap , ['] over , ['] + ,
  ['] over , ['] r@ , ['] - ,
  ['] over , ['] r@ , ['] - ,
  ['] xor , ['] 0< ,
  ['] rot , ['] drop ,
  ['] r> , ['] swap ,
  ['] 0branch , here 0 ,
  ['] drop , ['] drop ,
  ['] branch , here 0 ,
  swap here swap !
  ['] >r , ['] >r , ['] branch , swap ,
  here swap !
  (resolve-leaves) ; immediate

: i      ['] r@ , ; immediate
: unloop ['] r> , ['] drop , ['] r> , ['] drop , ; immediate
: j
  ['] r> , ['] r> , ['] r@ , ['] swap , ['] >r , ['] swap , ['] >r ,
  ; immediate
: leave
  ['] r> , ['] drop , ['] r> , ['] drop ,
  ['] branch , here leave-link @ , leave-link ! ; immediate

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
: within over - >r - r> u< ;

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

\ postpone appends compilation semantics: an immediate word's xt is
\ compiled directly; a normal word gets lit xt , so the definer
\ compiles it later. >counted stages the name for find in the free
\ space past here -- transient, overwritten by the next comma.

: >counted dup here c! here 1+ swap cmove here ;
: postpone
  parse-name >counted find ?dup 0= if
    drop ." postpone: word not found" cr
  else
    1 = if , else ['] lit , , ['] , , then
  then ; immediate

\ double-cell arithmetic over um* and um/mod

: dnegate invert swap invert 1+ swap over 0= if 1+ then ;
: dabs dup 0< if dnegate then ;
: m* 2dup xor 0< >r abs swap abs um* r> if dnegate then ;
: sm/rem
  2dup xor 0< >r
  over 0< >r
  abs >r dabs r> um/mod
  swap r> if negate then swap
  r> if negate then ;
: fm/mod
  dup >r sm/rem
  swap dup 0<> over r@ xor 0< and if
    r@ + swap 1-
  else
    swap
  then
  r> drop ;
: */mod >r m* r> sm/rem ;
: */ */mod nip ;

\ pictured numeric output. Digits are held from the top of a fixed
\ buffer downward; a full double in base 2 needs 128 chars plus sign.

create holdbuf 136 allot
variable hld

: mu/mod >r 0 r@ um/mod r> swap >r um/mod r> ;
: <# holdbuf 136 + hld ! ;
: hold hld @ 1- dup hld ! c! ;
: # base @ mu/mod rot dup 9 > if 7 + then [char] 0 + hold ;
: #s begin # 2dup or 0= until ;
: sign 0< if [char] - hold then ;
: #> 2drop hld @ holdbuf 136 + over - ;

: decimal 10 base ! ;
: hex 16 base ! ;

\ numeric display, burned down from C: the kernel's . is gone and
\ these are the real thing.

: u. 0 <# #s #> type space ;
: . dup abs 0 <# #s rot sign #> type space ;

\ numeric input: accumulate digits into a double in BASE.

: m+ swap >r dup >r + dup r> u< negate r> + ;
: ud* tuck * >r um* r> + ;
: >digit
  dup [char] 0 [char] : within if [char] 0 - -1 exit then
  dup [char] a [char] { within if [char] a - 10 + -1 exit then
  dup [char] A [char] [ within if [char] A - 10 + -1 exit then
  0 ;
: >number
  begin dup 0> while
    over c@ >digit 0= if drop exit then
    dup base @ < 0= if drop exit then
    >r 2swap base @ ud* r> m+ 2swap
    swap 1+ swap 1-
  repeat ;
