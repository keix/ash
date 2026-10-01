# Testing Ash

Ash's tests are split along a fourth boundary, as real as the three in
[DESIGN.md](DESIGN.md): C tests verify the kernel's structure, Forth
tests verify the language's observable semantics, and session tests
verify the interpreter shell itself.

```text
tests/c/          kernel structure        make test-c
──────────────── implementation boundary
tests/*.fs        word semantics          make test-forth
tests/session/    interpreter behavior    make test-session
```

`make test` runs all three layers. A change is done when all of them
pass.

## Layer 1 — C tests (`tests/c/`)

What belongs here: everything the Forth layer cannot observe.
Dictionary layout invariants (`xt` alignment, the does cell at
`xt[-1]`, body placement), name-length boundaries, lookup rules,
threading and unwinding through the NULL-ip sentinel, the code-field
routines, and later the JIT emitter.

Each file is a standalone `main()` built against every kernel object
except `main.o`, using plain `assert`. It prints `test_<name>: ok` on
success. Keep tests independent: a fresh `vm_t` per file, static
memory, no shared state.

## Layer 2 — Forth tests (`tests/*.fs`)

Hayes-style tests over `tests/tester.fs`:

```forth
t{ 1 2 + -> 3 }t
t{ 1 2 swap -> 2 1 }t
```

The runner interprets `tester.fs`, the test files, then `summary.fs`,
which prints `N tests: all forth tests passed` or a failure count —
the Makefile greps for the pass line.

Two rules keep this layer honest:

- **The tester and all test files are standard Forth only.** No Ash
  internals (`lit`, `branch`, `(does>)`, poked addresses). This is
  what makes the gforth oracle possible.
- **A word added to core.fs gets its `t{ -> }t` cases in the same
  commit**, alongside the STATUS.md update. Same discipline, same
  commit.

Known limits, by construction: this layer cannot check printed output
(`t{ -> }t` compares stacks), and it cannot test error recovery — an
error discards the rest of the source file. Both belong to layer 3.

## Layer 3 — session tests (`tests/session/`)

Expected-output transcripts: each `.fs` is piped to ash's stdin and
the combined stdout/stderr is diffed against the matching
`.expected`. The ` ok` prompts and stderr messages are part of the
transcript on purpose — this layer tests the REPL as a program.

What belongs here: error reporting and recovery (`undefined word`,
`: bad name`), stack-misuse detection, everything that prints
(`emit`, `type`, `.`, `."`), and boundary behavior. Every session
file ends with `depth .` expecting `0`, so a leaked cell anywhere in
the session fails the suite — the dynamic stand-in for the
stack-effect checking Forth cannot do statically.

Regeneration discipline: never regenerate an `.expected` blindly.
Run the suite first, read the diff, confirm every changed line is an
intended consequence, and only then save. A regenerated expectation
is an assertion, not a snapshot.

## The gforth oracle

```sh
make test-gforth
```

runs the same tester and the same layer-2 files under gforth. This
checks two things at once: that the tests really are standard Forth,
and that Ash's semantics agree with an independent, conforming
implementation. Compare only ANS-specified observable behavior —
never gforth-specific output or extensions.

Passing is a necessary signal, not a proof: as Forth-2012 Annex F
itself notes, no test suite can demonstrate conformance. The oracle
bounds our misunderstandings; it does not certify the system.

## Where does a new test go?

| The test needs… | Layer |
|---|---|
| a pointer into a header, a C type, a layout offset | C |
| only stack in, stack out | Forth (`t{ -> }t`) |
| to see printed characters | session |
| to survive an error and continue | session |
| to run under gforth too | Forth |

When in doubt, prefer layer 2 — it is the cheapest to write and the
only one the oracle checks.

## The ANS suite

```sh
make test-ans
```

runs the official Forth-2012 test suite's Core tests — `prelimtest.fth`,
then `core.fr` and `coreplustest.fth` over the suite's own `tester.fr`
— and fails on any reported error or undefined word. These are the
acceptance gate for the "ANS Forth System" label claimed in
[SPEC.md](SPEC.md).

The suite is not vendored. The Makefile expects a checkout of
`forth-standard-test-suite` beside the Ash tree; point `ANS_SUITE` at
its `src/` directory if it lives elsewhere. Because of that outside
dependency, `make test` does not run it: run `make test-ans` whenever
a change touches Core semantics.
