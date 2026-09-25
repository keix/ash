# Ash Status

The tracker for what Ash can and cannot do yet. The measure is the ANS
Forth Core word set (133 words); each implemented word is marked by
where it lives, because the ratio is the point: **C should shrink,
Forth should grow.**

Update this file in the same commit that adds or moves a word.

## Scoreboard

```text
ANS Core coverage   55 / 133
  in C              32
  in Forth          23
beyond Core         12  (7 Forth, 5 C internals)
```

## Core words implemented in C — the bootstrapping boundary

```text
! ' * + , - . / 0= : ; < = > >in >r @
dup drop execute exit find here immediate mod over r> r@ rot
state swap [']
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
postpone literal recurse [ ]
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
source evaluate quit abort abort" environment? (
```

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
- Word names are at most 255 chars; `dict_header` rejects longer ones
  and `:` reports them.
- `base` outside 2..36 never becomes C UB: `parse_number` rejects the
  token, `.` falls back to decimal.
- The physical ends of the stacks and dictionary live in the VM.
  `allot` fails fatally at the dictionary's end; stack misuse is
  detected between tokens at the outer interpreter — after the fact,
  and a single word can still run past the ends unchecked. The inner
  loop stays uninstrumented.
- A `code_t` function pointer is assumed to fit one cell (POSIX,
  x86_64 first); `ash.h` enforces it with a `_Static_assert`.
- `forth/core.fs` is loaded relative to the working directory.
- Control-flow words are not protected against interpret-state use.
