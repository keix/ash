# Ash

An ANS Forth implementation in C with a JIT compiler.

## Why Ash?

Ash begins with a small C kernel and grows in Forth.

The C layer provides only what is necessary to make Forth exist. Everything else should be written in Forth whenever possible.

Like ash left after a fire, the kernel is what remains after everything unnecessary has been removed.

A small C kernel. The rest is Forth.

## Philosophy

It’s just pointer magic in C.

## Platform

Ash is implemented in C11 and targets POSIX systems.

The threaded interpreter and core runtime are intended to be portable across supported POSIX environments.

JIT compilation is architecture-specific. The initial JIT target is x86_64.

```
Implementation language: C11
Runtime platform:        POSIX
Initial JIT target:      x86_64
```

Forth semantics remain independent of the underlying JIT architecture.

## Design

Ash is built around three explicit boundaries: semantic, execution, and optimization.

See [docs/DESIGN.md](docs/DESIGN.md) for the execution model, dictionary layout, threaded interpreter, and C/Forth bootstrapping boundary.

## Conformance

Ash provides the ANS Forth Core and Core Extensions word sets in full, the Exception and Exception Extensions word sets, and selected words from the Double-Number, String, and Programming-Tools Extensions. It passes the Forth 2012 test suite's Core and Core Extensions tests:

```sh
make test-ans
```

See [docs/SPEC.md](docs/SPEC.md) for the implementation-defined options and ambiguous-condition behavior the label obliges.

## Development

A Nix development shell is recommended for a reproducible environment.

```sh
nix develop
```

Ash is built with `make`.

```sh
make
```

The default compiler is `cc`. Both GCC and Clang should be supported.

```sh
make CC=gcc
make CC=clang
```

The project targets C11 and POSIX and avoids compiler-specific extensions where possible.

## Acknowledgments

Thanks to the ANS Forth committee and the Forth community for their
work on the standard, to the maintainers of the Forth 2012 test suite
that gates Ash's conformance, to the Gforth developers for the
implementation used as a reference in Ash's tests, and to the GNU
Project for its commitment to software freedom.

## License

Copyright KEI SAWAMURA 2026.  
Ash is licensed under the MIT License. Copying and modifying is encouraged and appreciated.
