# Ash
An ANS Forth implementation in C.

## Why Ash?
Ash begins with a small C kernel and grows in Forth.

The C layer provides only what is necessary to make Forth exist. Everything else should be written in Forth whenever possible.

Like ash left after a fire, the kernel is what remains after everything unnecessary has been removed.

A small C kernel. The rest is Forth.

## Design

Ash is divided by responsibility, not convenience.

The C kernel owns the boundary to the machine:

- data and return stacks
- memory access
- dictionary representation and lookup
- token input
- threaded execution
- primitive words that cannot reasonably be expressed in Forth

The Forth layer builds the language above that boundary.

Derived words, control structures, utilities, and as much of the ANS Forth environment as possible are implemented in Forth itself.

The initial execution model is threaded and deliberately simple.

JIT compilation is an optimization of that execution model, not a separate language or runtime. Hot Forth words may later be translated into native code while preserving the same semantics.

The boundary should remain small, explicit, and replaceable.
