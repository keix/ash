# Ash Status

The tracker for what Ash can and cannot do yet. The measure is the ANS
Forth Core word set (133 words); each implemented word is marked by
where it lives, because the ratio is the point: **C should shrink,
Forth should grow.**

Update this file in the same commit that adds or moves a word.

## Scoreboard

```text
ANS Core coverage   133 / 133 — complete
  in C              52
  in Forth          81
beyond Core         59  (42 Forth, 17 C)

Forth-2012 suite    prelimtest.fth   0 of 57 failed
                    core.fr          0 failures
                    coreplustest.fth 0 failures

JIT                 hot words burn by default: counters outside the
                    dictionary (2-way set-associative, keyed by xt
                    for calls and by target for taken backward
                    branches) compile a word at its 512th call or
                    512th loop iteration; a loop burns its word
                    mid-call and resumes natively at the branch
                    target. jit-on = eager at ; · jit-off = off.
                    Every suite passes in all three modes.
                    The segment caches keep the top four cells in
                    registers or as constants (the top also as
                    condition codes) and rsp in a register: stack
                    shuffles are renames, literals immediates, and
                    a compare feeds its branch.
                    -O2, one session, cpu-pinned, medians of 3,
                    default mode · gforth / gforth-fast same session:
                    fib(40)  0.93s (jit-off 8.16s) · 1.08s / 0.50s
                    loop     0.015s (was 1.02s before loops counted
                             as hot) · 0.029s / 0.016s
                    sieve    0.034s (was 0.89s) · 0.12s / 0.034s
                    prior sessions: python 3.14 7.05s on fib
```

The dot moved: `.` was a C primitive and is now Forth over pictured
numeric output — the first word burned out of the kernel.

## Core words implemented in C — the bootstrapping boundary

```text
! ' * + , - / 0= : ; < = > >in >r @
accept align allot and base c! c, c@ create depth dup drop
emit execute exit find here immediate invert key lshift mod or
over quit r> r@ rot rshift source state swap um* um/mod xor
[']
```

## Core words implemented in Forth — core.fs

```text
# #> #s ( ." */ */mod +! +loop . /mod 0< 1+ 1- 2! 2* 2/ 2@
2drop 2dup 2over 2swap <# >body ?dup [ ] [char] abs aligned
begin bl cell+ cells char char+ chars constant count cr
abort abort" decimal do does> else environment? evaluate fill
fm/mod hold i if j leave literal loop m* max min move negate
postpone recurse repeat s" s>d sign sm/rem space spaces then
type u. u< unloop until variable while word >number
```

## Core words missing — grouped by what unblocks them

None. The Core word set is complete. The text interpreter itself is
Forth (`interpret` in core.fs); `evaluate` wraps it in `catch` so a
throw unwinds the source stack one level at a time and rethrows.
The Forth-2012 test suite gated the conformance label; `make test-ans`
passes, and the label is claimed in [SPEC.md](SPEC.md).

## Beyond Core

Implemented from Core Ext and elsewhere:

```text
Forth:  nip tuck <> 0<> 0> .( :noname ?do again false hex true
        within                    (Core Ext)
        catch throw               (Exception word set)
        cmove cmove>              (String word set)
        dnegate dabs m+           (Double word set)
        <= >= cell -rot           (common practice, not ANS)
        interpret                 (common practice, not ANS)
        ,string latest-xt mu/mod ud* >digit >counted
        leave-link (resolve-leaves) ,msb ,do-setup ,loop-check
        handler uncaught abort-msg abort-len (abort") (number)
                                  (Ash internals)
C:      \ parse  (Core Ext)   bye  (Tools Ext)
        parse-name                (Forth-2012)
        sp@ sp! rp@ rp!           (common practice, not ANS)
        latest lit branch 0branch (does>) (s") (abort)
        (push-source) (pop-source) jit jit-on jit-off
                                  (Ash internals)
```

## Deviations and spec decisions

Moved to [SPEC.md](SPEC.md): Ash's answers to the standard's
implementation-defined options and ambiguous conditions live there and
change only with design decisions. This file tracks only coverage.
