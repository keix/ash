# Ash Status

The tracker for what Ash can and cannot do yet. The measure is the ANS
Forth Core word set (133 words); each implemented word is marked by
where it lives, because the ratio is the point: **C should shrink,
Forth should grow.**

Update this file in the same commit that adds or moves a word.

## Scoreboard

```text
ANS Core coverage   51 / 133
  in C              28
  in Forth          23
beyond Core         12  (7 Forth, 5 C internals)
```

## Core words implemented in C — the bootstrapping boundary

```text
! ' * + , - . / 0= : ; < = > >r @
dup drop exit here immediate mod over r> r@ rot swap [']
```

## Core words implemented in Forth — core.fs

```text
0< 1+ 1- 2* 2/ 2drop 2dup ?dup
abs begin do else i if loop max min negate
repeat then unloop until while
```

## Core words missing — grouped by what unblocks them

Character I/O and strings (needs `emit`, `key`, string literals):

```text
." s" emit type cr space spaces key accept
bl char [char] count word
```

Pictured numeric output (needs `base` as a word and a hold area;
burning the C `.` down to Forth rides on this):

```text
# #> #s <# hold sign u. decimal base >number
```

Defining words and the compiler surface (needs `create`, the does
cell, and `state`/`postpone` exposure):

```text
create does> variable constant >body
postpone literal recurse [ ] state find execute
```

Memory, characters, and data space:

```text
c! c@ c, cell+ cells char+ chars align aligned allot
fill move 2! 2@ +! 2over 2swap depth
```

Arithmetic and logic:

```text
and or xor invert lshift rshift
*/ */mod /mod m* um* um/mod fm/mod sm/rem s>d u< +loop j leave
```

Interpreter and system:

```text
>in source evaluate quit abort abort" environment? (
```

Note: `execute`, `find`, `state`, and `>in` already exist inside the C
kernel; they are missing only as dictionary words.

## Beyond Core

Implemented from Core Ext and elsewhere:

```text
Forth:  nip tuck <> 0> again      (Core Ext)
        <= >=                     (common practice, not ANS)
C:      \  (Core Ext)   bye  (Tools Ext)
        lit branch 0branch        (Ash internals, not ANS)
```

## Deviations and open spec items

Recorded here until docs/ASH_SPEC.md exists:

- Dictionary lookup is ASCII case-insensitive.
- `/` and `mod` are symmetric (C truncation); revisit with `fm/mod`
  and `sm/rem`.
- No stack under/overflow detection yet.
- `forth/core.fs` is loaded relative to the working directory.
- Control-flow words are not protected against interpret-state use.
