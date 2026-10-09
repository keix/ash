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
\ 2/ is defined after the compiler surface: it needs [ ] literal

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

\ Loop parameters ride the return stack biased by min-int:
\ x = index - limit + min-int with s = limit - min-int beneath it,
\ so i is x + s, and the loop ends exactly when x + step overflows
\ signed -- the ANS boundary between limit-1 and limit, correct at
\ any step size including wraparound.

: ,msb ['] lit , 1 cell 8 * 1- lshift , ;
: ,do-setup
  ['] over , ,msb ['] - , ['] >r ,
  ['] swap , ['] - , ,msb ['] + , ['] >r , ;
: ,loop-check
  ['] r> , ['] 2dup , ['] + , ['] >r ,
  ['] 2dup , ['] xor , ['] invert ,
  ['] swap , ['] r@ , ['] xor , ['] and , ['] nip , ['] 0< ,
  ['] 0branch , here 0 ,
  ['] r> , ['] drop , ['] r> , ['] drop ,
  ['] branch , here 0 ,
  swap here swap !
  ['] branch , swap ,
  here swap !
  (resolve-leaves) ;

: do  leave-link @ 0 leave-link ! ,do-setup here ; immediate

: ?do leave-link @ 0 leave-link !
      ['] 2dup , ['] = ,
      ['] 0branch , here 0 ,
      ['] drop , ['] drop ,
      ['] branch , here leave-link @ , leave-link !
      here swap !
      ,do-setup here ; immediate

: loop ['] lit , 1 , ,loop-check ; immediate
: +loop ,loop-check ; immediate

: i ['] r> , ['] dup , ['] r@ , ['] + , ['] swap , ['] >r , ; immediate
: unloop ['] r> , ['] drop , ['] r> , ['] drop , ; immediate
: j
  ['] r> , ['] r> , ['] r> , ['] dup , ['] r@ , ['] + ,
  ['] swap , ['] >r , ['] rot , ['] swap , ['] rot , ['] >r ,
  ['] swap , ['] >r ,
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

\ / mod /mod live after sm/rem now: division is forth
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
: entry>xt cell+ 1+ dup c@ swap 1+ + aligned cell+ ;
: latest-xt latest entry>xt ;

\ recurse compiles the definition being made, which :noname words do
\ not register in the dictionary -- so : itself, re-defined in forth
\ over the kernel's, records the xt of whatever is open.

variable current-xt
: : [ ' : ] literal execute latest-xt current-xt ! ;
: recurse current-xt @ , ; immediate

\ postpone appends compilation semantics: an immediate word's xt is
\ compiled directly; a normal word gets lit xt , so the definer
\ compiles it later. >counted stages the name for find in the free
\ space past here -- transient, overwritten by the next comma.

: >counted dup here c! here 1+ swap cmove here ;

\ :noname lays a headerless code field -- does cell, then docol
\ copied at compile time from any colon word's code field -- and
\ leaves the xt on the stack for the ; that ends the definition.

: :noname align 0 , here dup current-xt ! [ ' nip @ ] literal , ] ;
: postpone
  parse-name >counted find ?dup 0= if
    drop ." postpone: word not found" cr
  else
    1 = if , else ['] lit , , ['] , , then
  then ; immediate


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

\ exceptions: non-local return as pure return-stack surgery, no
\ setjmp. catch records both stack pointers; throw restores them and
\ every threaded continuation in between simply ceases to exist.

variable handler
0 handler !
variable abort-msg
variable abort-len

: catch
  sp@ >r handler @ >r rp@ handler !
  execute
  r> handler ! r> drop 0 ;

: uncaught
  dup -1 = if drop else
  dup -2 = if drop abort-msg @ abort-len @ type cr else
  ." uncaught throw: " . cr then then
  (abort) ;

: throw
  ?dup if
    handler @ 0= if uncaught then
    handler @ rp! r> handler !
    r> swap >r sp! drop r>
  then ;

: abort -1 throw ;
: (abort") rot if abort-len ! abort-msg ! -2 throw then 2drop ;
: abort" postpone s" postpone (abort") ; immediate

\ double-cell arithmetic over um* and um/mod

: dnegate invert swap invert 1+ swap over 0= if 1+ then ;
: dabs dup 0< if dnegate then ;
: m* 2dup xor 0< >r abs swap abs um* r> if dnegate then ;
: sm/rem
  dup 0= if -10 throw then
  2dup xor 0< >r
  over 0< >r
  abs >r dabs r> um/mod
  swap r> if negate then swap
  r> if negate then ;
\ division, burned out of the kernel: the C prims are gone and
\ everything routes through sm/rem, where zero throws -10.

: /mod >r s>d r> sm/rem ;
: / /mod nip ;
: mod /mod drop ;

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

\ input words. word skips leading delimiters by peeking the source
\ through >in, then parses and stages a counted string past here.

: word
  >r
  begin
    source nip >in @ >
    if source drop >in @ + c@ r@ = else 0 then
  while 1 >in +! repeat
  r> parse >counted ;

\ every environmental query may answer unknown (Forth-2012)
: environment? 2drop 0 ;

\ 2/ is an arithmetic shift, not division: the sign bit propagates,
\ so -1 2/ is -1. The mask is computed once, at compile time.

: 2/ dup 1 rshift swap 0< [ 1 cell 8 * 1- lshift ] literal and or ;

0 constant false
-1 constant true
: .( 41 parse type ; immediate

\ the text interpreter, in forth. A failed conversion throws -13 and
\ needs no stack cleanup: catch's sp! discards the debris. evaluate
\ wraps interpret in catch so a throw pops the source stack one
\ level at a time and rethrows -- no C frames, no leaks.

\ keep in sync with parse_number in interp.c: 'c' char literals and
\ the Forth-2012 prefixes # $ % with the sign after the prefix.

: (number)
  dup 0= if -13 throw then
  dup 3 = if
    over dup c@ [char] ' = swap 2 + c@ [char] ' = and if
      drop 1+ c@ exit
    then
  then
  base @ >r
  over c@ [char] # = if 10 base ! swap 1+ swap 1- else
  over c@ [char] $ = if 16 base ! swap 1+ swap 1- else
  over c@ [char] % = if  2 base ! swap 1+ swap 1- then then then
  dup 0= if r> base ! -13 throw then
  over c@ [char] - = over 1 > and
  dup >r if 1- swap 1+ swap then
  0 0 2swap >number
  nip 0<> if r> drop r> base ! -13 throw then
  drop r> if negate then r> base ! ;

variable last-tok
variable last-len
create tok-buf 260 allot

\ stage the name off here, so a word being interpreted never clobbers
\ the data-space pointer a program may be building on
: >tok dup tok-buf c! tok-buf 1+ swap cmove tok-buf ;

: interpret
  begin parse-name dup while
    2dup last-len ! last-tok !
    2dup >tok find ?dup if
      2swap 2drop
      state @ if
        1 = if execute else , then
      else drop execute then
    else
      drop (number)
      state @ if postpone literal then
    then
    depth 0< if -4 throw then
  repeat 2drop ;

: evaluate -1 (push-source) ['] interpret catch (pop-source) throw ;

\ core extension words, all forth

: u> swap u< ;
: pick 1+ cells sp@ + @ ;
: roll dup 0= if drop exit then swap >r 1- recurse r> swap ;

\ the pairs are compiled inline so the caller's return frame is not
\ in the way, like i and unloop

: 2>r ['] swap , ['] >r , ['] >r , ; immediate
: 2r> ['] r> , ['] r> , ['] swap , ; immediate
: 2r@ ['] r> , ['] r> , ['] 2dup , ['] >r , ['] >r , ['] swap ,
  ; immediate

\ case: the selector stays on the stack; of compares a copy; endcase
\ resolves every endof's forward branch down to case's 0 sentinel.
\ Note endcase drops one cell: a default clause must leave the
\ selector (or a replacement) for it --
\   case 1 of ... endof 2 of ... endof ( default, selector on top )
\   endcase

: case 0 ; immediate
: of postpone over postpone = postpone if postpone drop ; immediate
: endof postpone else ; immediate
: endcase
  postpone drop begin ?dup while postpone then repeat ; immediate

: value create , does> @ ;
: to ' >body state @ if postpone literal postpone ! else ! then
  ; immediate
: buffer: create allot ;

\ marker: the word's own body remembers where here and latest stood
\ just before its header; executing it puts them back.

: here! here - allot ;
: marker
  here latest create swap , ,
  does> dup @ swap cell+ @ latest! here! ;

: pad here 512 + ;

\ s" grows interpretation semantics: the string is copied to one of
\ two alternating transient regions above pad, as the standard asks

variable s"-buf
: s"
  state @ if ['] (s") , ,string exit then
  34 parse
  s"-buf @ if 1024 else 1536 then
  s"-buf @ 0= s"-buf !
  here +
  swap >r swap over r@ cmove
  r> ; immediate
: compile, , ;
: u.r >r 0 <# #s #> r> over - 0 max spaces type ;
: .r >r dup abs 0 <# #s rot sign #> r> over - 0 max spaces type ;

\ c" lays the count twice in the length cell: the low byte for the
\ skip, the top byte as the count byte right before the characters.

: c" ['] (c") , 34 parse dup dup 56 lshift or ,
  begin dup 0> while swap dup c@ c, 1+ swap 1- repeat 2drop align
  ; immediate

: erase 0 fill ;
: holds begin dup while 1- 2dup + c@ hold repeat 2drop ;

\ deferred words. The does-thread is "@ execute", which const_like
\ does not fold -- a deferred word stays honestly indirect.

: defer create ['] abort , does> @ execute ;
: defer! >body ! ;
: defer@ >body @ ;
: is
  state @ if postpone ['] postpone defer! else ' defer! then
  ; immediate
: action-of
  state @ if postpone ['] postpone defer@ else ' defer@ then
  ; immediate

\ s\" with the Forth-2012 escapes, translated while copying; \m lays
\ two characters, \x two hex digits, anything unknown is itself.

\ the input-source words over >in and the source stack. refill is
\ false for every source until the terminal loop moves to forth.

: save-input >in @ 1 ;
: restore-input drop >in ! 0 ;
: refill 0 ;
: [compile] ' , ; immediate

: src-c@+ source drop >in @ + c@ 1 >in +! ;

: s\"
  ['] (s") , here 0 , 0
  begin src-c@+ dup [char] " <> while
    dup [char] \ = if
      drop src-c@+ dup case
        [char] a of drop 7 endof
        [char] b of drop 8 endof
        [char] e of drop 27 endof
        [char] f of drop 12 endof
        [char] l of drop 10 endof
        [char] n of drop 10 endof
        [char] q of drop 34 endof
        [char] r of drop 13 endof
        [char] t of drop 9 endof
        [char] v of drop 11 endof
        [char] z of drop 0 endof
        [char] " of drop 34 endof
        [char] \ of drop 92 endof
        [char] m of drop 13 c, 1+ 10 endof
        [char] x of drop
          src-c@+ >digit drop 16 * src-c@+ >digit drop + endof
      endcase
    then
    c, 1+
  repeat drop
  swap ! align ; immediate

\ the quit loop, in forth: refill the terminal, interpret, report.
\ Its catch is the standing top-level handler, so every repl error
\ arrives as a throw; -13 names the token interpret just recorded.

create tib 1024 allot

: refill
  source-id 0= if
    tib 1024 accept
    dup 0= (eof?) and if drop false exit then
    tib swap (set-source) true
  else false then ;

: report
  dup -13 = if drop ." undefined word: " last-tok @ last-len @ type cr
  else dup -4 = if drop ." stack underflow" cr
  else dup -1 = if drop
  else dup -2 = if drop abort-msg @ abort-len @ type cr
  else ." uncaught throw: " . cr
  then then then then
  begin depth 0> while drop repeat
  0 state !
  source nip >in ! ;

: interpret-line ['] interpret catch ?dup if report then ."  ok" cr ;
: (quit-loop) begin refill while interpret-line repeat bye ;
: quit 0 handler ! rp0 rp! (quit-loop) ;

\ compile every colon word so far to native; the code field is the
\ only thing that changes, so order does not matter.

: jit-all latest begin ?dup while dup entry>xt jit @ repeat ;
