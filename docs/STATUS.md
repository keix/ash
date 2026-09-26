# Ash Status

The tracker for what Ash can and cannot do yet. The measure is the ANS
Forth Core word set (133 words); each implemented word is marked by
where it lives, because the ratio is the point: **C should shrink,
Forth should grow.**

Update this file in the same commit that adds or moves a word.

## Scoreboard

```text
ANS Core coverage   73 / 133
  in C              39
  in Forth          34
beyond Core         14  (8 Forth, 6 C internals)
```

## Core words implemented in C — the bootstrapping boundary

```text
! ' * + , - . / 0= : ; < = > >in >r @
c! c, c@ create depth dup drop emit execute exit find here
immediate key mod over r> r@ rot state swap [']
```

## Core words implemented in Forth — core.fs

```text
0< 1+ 1- 2* 2/ 2drop 2dup >body ?dup
abs begin bl cell+ cells constant cr do does> else i if loop
max min negate repeat space spaces then type unloop until
variable while
```

## Core words missing — grouped by what unblocks them

Strings (needs a parse primitive for `"`-delimited text):

```text
." s" accept char [char] count word
```

Pictured numeric output (needs `base` as a word and a hold area;
burning the C `.` down to Forth rides on this):

```text
# #> #s <# hold sign u. decimal base >number
```

Defining words and the compiler surface:

```text
postpone literal recurse [ ]
```

Memory, characters, and data space:

```text
char+ chars align aligned allot
fill move 2! 2@ +! 2over 2swap
```

Arithmetic and logic:

```text
and or xor invert lshift rshift
*/ */mod /mod m* um* um/mod fm/mod sm/rem s>d u< +loop j leave
```

Interpreter and system:

```text
source evaluate quit abort abort" environment? (
```

## Beyond Core

Implemented from Core Ext and elsewhere:

```text
Forth:  nip tuck <> 0> again      (Core Ext)
        <= >= cell                (common practice, not ANS)
C:      \  (Core Ext)   bye  (Tools Ext)
        lit branch 0branch (does>)  (Ash internals, not ANS)
```

## Deviations and spec decisions

Moved to [SPEC.md](SPEC.md): Ash's answers to the standard's
implementation-defined options and ambiguous conditions live there and
change only with design decisions. This file tracks only coverage.
