# Ash Status

The tracker for what Ash can and cannot do yet. The measure is the ANS
Forth Core word set (133 words); each implemented word is marked by
where it lives, because the ratio is the point: **C should shrink,
Forth should grow.**

Update this file in the same commit that adds or moves a word.

## Scoreboard

```text
ANS Core coverage   97 / 133
  in C              47
  in Forth          50
beyond Core         19  (10 Forth, 9 C)
```

## Core words implemented in C — the bootstrapping boundary

```text
! ' * + , - . / 0= : ; < = > >in >r @
align allot and c! c, c@ create depth dup drop emit execute
exit find here immediate invert key lshift mod or over r> r@
rot rshift state swap xor [']
```

## Core words implemented in Forth — core.fs

```text
( ." +! 0< 1+ 1- 2! 2* 2/ 2@ 2drop 2dup 2over 2swap
>body ?dup [char] abs aligned begin bl cell+ cells char char+
chars constant count cr do does> else fill i if literal loop
max min negate repeat s" space spaces then type unloop until
variable while
```

## Core words missing — grouped by what unblocks them

Strings and terminal input:

```text
accept word
```

Pictured numeric output (needs `base` as a word and a hold area;
burning the C `.` down to Forth rides on this):

```text
# #> #s <# hold sign u. decimal base >number
```

Defining words and the compiler surface:

```text
postpone recurse [ ]
```

Memory:

```text
move
```

Arithmetic and loops:

```text
*/ */mod /mod m* um* um/mod fm/mod sm/rem s>d u< +loop j leave
```

Interpreter and system:

```text
source evaluate quit abort abort" environment?
```

## Beyond Core

Implemented from Core Ext and elsewhere:

```text
Forth:  nip tuck <> 0> again      (Core Ext)
        <= >= cell -rot           (common practice, not ANS)
        ,string                   (Ash internal)
C:      \ parse  (Core Ext)   bye  (Tools Ext)
        parse-name                (Forth-2012)
        lit branch 0branch (does>) (s")  (Ash internals, not ANS)
```

## Deviations and spec decisions

Moved to [SPEC.md](SPEC.md): Ash's answers to the standard's
implementation-defined options and ambiguous conditions live there and
change only with design decisions. This file tracks only coverage.
