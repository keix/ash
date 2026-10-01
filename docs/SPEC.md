# Ash Specification

ANS Forth requires a system to document its implementation-defined
options and its behavior under ambiguous conditions. This file is that
document: Ash's answers to the standard's blanks.

What Ash *can do* today is [STATUS.md](STATUS.md); what Ash *is* — the
decisions and promises — lives here. Entries change only when a design
decision changes, in the same commit.

## System

Ash targets the ANS Forth Core word set, with selected later-standard
words where they simplify the system. The Core word set is complete
(133/133); the label "ANS Forth System" will be claimed when it
passes the Forth-2012 test suite.

## Implementation-defined options

| Item | Ash's answer |
|---|---|
| cell size | 64 bits (`intptr_t`, x86_64 first); core.fs measures it at boot |
| char size | 1 byte, ASCII |
| alignment | cell alignment, 8 bytes; headers and code fields are cell-aligned |
| case sensitivity | dictionary lookup is ASCII case-insensitive |
| division rounding | `/ mod /mod */ */mod` are symmetric; both `fm/mod` (floored) and `sm/rem` (symmetric) are provided |
| flags | true = -1, false = 0 |
| number conversion | radix is `BASE`, valid 2..36; digits `0-9` then `a-z`/`A-Z`; Forth-2012 prefixes `#` `$` `%` and `'c'` literals, sign after the prefix |
| word name length | 1..255 characters |
| data stack | 1024 cells |
| return stack | 1024 cells |
| data space | one contiguous region, 64 KiB |
| input sources | terminal and files line by line (`SOURCE` is the current line), string; nesting depth 8 |
| `SOURCE-ID` values | 0 = terminal, -1 = string |
| whitespace | any char <= 0x20 delimits tokens |
| `environment?` | every query answers unknown (false), as Forth-2012 permits |

## Ambiguous conditions

| Condition | Behavior |
|---|---|
| undefined word | message to stderr; data stack cleared, compile state left, rest of the parse area discarded |
| stack underflow / overflow | detected between tokens at the outer interpreter — after the fact; recovery as above. A single word can still run past the physical ends unchecked; the inner loop is uninstrumented |
| dictionary overflow | `allot` reports `dictionary full` and exits (fatal) |
| name empty or over 255 chars | rejected; `:` and `create` report and discard the line |
| `BASE` outside 2..36 | never becomes C UB: number parsing rejects the token, `.` falls back to decimal |
| division by zero | undetected; inherits the platform's behavior |
| compile-only word interpreted | unprotected; corrupts data space silently |
| uncaught `throw` | -1 aborts silently; -2 prints the stored `abort"` message; other codes print `uncaught throw: n`. All clear both stacks and discard the parse area |

## Platform assumptions beyond ISO C

- POSIX; the initial architecture is x86_64.
- A `code_t` function pointer fits in one cell; `ash.h` enforces this
  with a `_Static_assert`.
- The kernel itself is endianness-neutral; some tests poke counted
  strings as little-endian cells.

## Environmental restrictions

- `forth/core.fs` is loaded relative to the working directory.

## Extensions beyond the Core word set

- From other word sets: `\` (Core Ext), `nip tuck <> 0> again`
  (Core Ext), `bye` (Tools Ext).
- Common practice, not ANS: `<=`, `>=`, `cell`.
- Ash internals exposed as words: `lit`, `branch`, `0branch`,
  `(does>)`.
- Interactive startup prints a license banner when stdin is a tty.
