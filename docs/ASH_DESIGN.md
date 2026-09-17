# Ash Design Note

This document describes the design of the C kernel and its boundary with the Forth layer.

## Three boundaries

The whole of Ash can be explained by three boundaries:

```text
┌──────────────────────────────────────────┐
│                                          │
│    source  ─────────────────▶  token     │
│                                          │
╞═══════════ semantic boundary ════════════╡
│                                          │
│    outer interpreter  ──────▶  xt        │
│                                          │
╞═══════════ execution boundary ═══════════╡
│                                          │
│    threaded engine  ────────▶  machine   │
│                                          │
╞══════════ optimization boundary ═════════╡
│                                          │
│    JIT                                   │
│                                          │
└──────────────────────────────────────────┘
```

Above the semantic boundary, text is only cut into tokens — no meaning is assigned. Meaning appears when the outer interpreter maps a token to an xt or a number. The execution boundary separates deciding *what* to run from running it. The optimization boundary is where a JIT may later replace threaded dispatch with native code, without changing anything above it.

## Standard target

Ash targets the ANS Forth Core word set, with selected later-standard words (e.g. `PARSE-NAME` from Forth-2012) where they simplify the system. `CATCH` and `THROW` belong to the optional Exception word set, but Ash builds them anyway: `ABORT` and `QUIT` are Core words and need the same machinery.

## Execution flow

Forth has almost no syntax. There is no lexer → parser → AST → compiler pipeline; the model is much closer to tokenizer → interpreter.

```text
input
  ↓
tokenizer            next_token()   — cuts tokens, nothing else
  ↓
token
  ↓
interpreter          interpret_token()
  ├─ found in dictionary → execute
  └─ parses as a number  → push onto data stack
```

For example, `1 2 +` proceeds as:

```text
"1"  → number → push 1
"2"  → number → push 2
"+"  → dictionary lookup → execute +
```

The tokenizer holds no judgment. All meaning lives in the interpreter. When the system enters compilation state, the token stream is unchanged — only the interpreter's response to a token changes:

```text
word found
  ├─ immediate → execute now
  └─ normal    → compile its xt into the current definition
```

This is the essential property of Forth: the parser barely changes, and state changes what interpretation means.

## Source layout

```text
src/
  ash.h      — cell type, VM state, dictionary entry definitions
  token.c    — next_token()  (input source → token)
  interp.c   — interpret_token(), outer interpreter loop
  exec.c     — inner interpreter (threaded execution)
  dict.c     — dictionary representation and find_word()
  stack.c    — data / return stacks
  prims.c    — primitive words forming the bootstrapping boundary
  main.c     — REPL and boot
forth/
  core.fs    — derived words, control structures (the Forth layer)
```

This mirrors the principle in the README: the C layer provides only what is necessary to make Forth exist.

## Core types

In Ash, the differences between a cell, an xt, a code field, a parameter field, and the instruction pointer *are* the execution model. The C type system should keep those boundaries visible rather than erase them all into `cell_t`:

```c
typedef struct vm vm_t;

typedef intptr_t  cell_t;                     /* one cell */
typedef cell_t   *xt_t;                       /* execution token: address of a code field */
typedef void    (*code_t)(vm_t *vm, xt_t xt); /* code-field routine */
```

This separation costs nothing now and pays off most once the JIT starts rewriting code fields.

```c
typedef struct {
    const char *buf;       /* input buffer */
    cell_t      len;
    cell_t      in;        /* >IN: parse position within buf */
    cell_t      source_id; /* 0 = terminal, -1 = string (EVALUATE), else fileid */
} input_source_t;

struct vm {
    cell_t  *dsp, *rsp;        /* data / return stack pointers */
    xt_t    *ip;               /* instruction pointer (threaded code) */
    uint8_t *here;             /* dictionary allocation pointer */
    cell_t   state;            /* 0 = interpret, -1 = compile */
    cell_t   base;             /* number conversion radix, initially 10 */
    input_source_t src[SOURCE_DEPTH];   /* input source stack */
    cell_t   src_depth;        /* src[src_depth] is the active source */
};
```

Decisions that matter here:

- **`state`, `base`, and `>IN` are plain cells in the VM.** The standard treats them as system data a program may access; keeping them as addressable cells means the Forth layer can touch them with `STATE @`, `BASE !`, or `>IN !`, so words like `[`, `]`, `HEX`, and `POSTPONE` can be written in Forth without extending the C kernel.
- **The dictionary is a single contiguous data space** through which `here` advances. `HERE`, `,`, and `ALLOT` fall out naturally, and the layout matches the ANS data-space semantics. Allocating entries individually (e.g. with malloc) would fight the language later.

## Input sources

The standard's input source is not the terminal; it is an abstraction over terminal, string, and file, with nested input that can be saved and restored. The TIB is therefore not the essence of the VM — the source abstraction is.

The active source is the top of a small stack of `input_source_t`. The tokenizer always reads from the active source; nothing else in the system knows where text comes from.

- `SOURCE` returns the active `buf len`; `>IN` is the active source's `in` cell; `SOURCE-ID` distinguishes terminal, string, and file.
- `EVALUATE` pushes a string source, runs the same outer interpreter loop, and pops it.
- `REFILL` asks the active source for its next line (terminal read, file read; false for a string).
- Bootstrapping `core.fs` is nothing special: push a file source at startup and run the ordinary outer loop until it is exhausted.

One outer loop, many sources. The interpreter does not change when the input does.

## Tokenizer

```c
/* Cut one whitespace-delimited token from the active input source,
   starting at its >IN. Returns a pointer into the source buffer
   (no copy). NULL when the source is exhausted. */
const char *next_token(vm_t *vm, size_t *len);
```

- Zero judgment: skip whitespace, delimit, return.
- Returning a pointer + length into the input buffer matches the standard's text-parsing model: the parse area begins at `>IN` and parsing advances it. Exported as a primitive, this is `PARSE-NAME` (a Forth-2012 word Ash adopts), and the Forth layer can build `WORD` and friends on top of it.

## Outer interpreter

```c
void interpret_token(vm_t *vm, const char *tok, size_t len) {
    dict_entry_t *w = find_word(vm, tok, len);
    if (w) {
        if (vm->state && !(w->flags & F_IMMEDIATE))
            compile_cell(vm, (cell_t)xt(w));   /* append xt to current definition */
        else
            execute(vm, xt(w));                /* immediate, or interpreting */
    } else if (parse_number(tok, len, &n)) {   /* radix = BASE */
        if (vm->state) {
            compile_cell(vm, (cell_t)xt_lit);  /* compile LIT n */
            compile_cell(vm, n);
        } else {
            push(vm, n);
        }
    } else {
        error(vm, "undefined word", tok, len);
    }
}
```

The outer loop is nothing more than:

```c
while ((tok = next_token(vm, &len)))
    interpret_token(vm, tok, len);
```

State affects only the two branches inside `interpret_token`. The token stream is identical in both states.

A note on conformance: the standard specifies per-word *interpretation semantics* and *compilation semantics* — for a normal definition, the compilation semantics are to append its execution semantics to the current definition. The `xt + IMMEDIATE flag` scheme above is not those semantics; it is **Ash's implementation model** of them. Ash initially represents a word's compilation behavior as an execution token plus an immediate flag. If a word ever requires semantics this model cannot express, it is the model that changes, not the standard.

## Dictionary layout

The dictionary is where every other concept — xt, code field, parameter field, `>BODY`, `CREATE`, `DOES>`, colon definitions, JIT — either connects or fails to. The physical layout of an entry:

```text
          ┌───────────────┐
entry ──▶ │ link          │ 1 cell    → previous entry (0 terminates)
          ├───────────────┤
          │ flags         │ 1 byte    IMMEDIATE | HIDDEN
          │ name_len      │ 1 byte
          │ name…         │ name_len bytes
          │ padding       │ → next cell boundary
          ├───────────────┤
          │ does          │ 1 cell    does-code address, 0 if unused   = xt[-1]
          ├───────────────┤
xt ─────▶ │ code          │ 1 cell    code_t                           = xt[0]
          ├───────────────┤
body ───▶ │ parameter     │ …         data field
          │ field         │
          └───────────────┘
```

All the address arithmetic is fixed and trivial:

```text
xt      = &code
>BODY   = xt + 1 cell
does    = xt[-1]
```

### Why `does` is not in the parameter field

Putting the does-code address in the parameter field looks attractive but conflicts with the standard. By the time `DOES>` applies — `: constant create , does> @ ;` — the data field already contains data, and the standard fixes `>BODY` to the data-field address assigned at `CREATE` time. Prepending the does-address would move the data; appending it would put it at a word-dependent offset the handler cannot find.

So the does-address lives in a fixed slot adjacent to the code field, at `xt[-1]`. The data field never moves, `>BODY` stays `xt + 1` for every word, and — the property that matters most — **the code field remains a single cell**: the one and only slot that changes when a word's execution strategy changes. The cost is one cell per entry.

### Word classes

```text
class        code        does      body
primitive    C routine    0         —
colon        docol        0         xt … exit
create       docreate     0         data field
constant     docon        0         value
does> word   dodoes       thread    data field (unmoved)
```

Runtime behavior of each code-field routine:

```text
docreate:  push body
docon:     push body[0]
docol:     rpush ip; ip = body
dodoes:    push body; rpush ip; ip = does     — data-field address, then DOES> semantics
```

`DOES>` itself is an immediate word that compiles `(does>)` into the defining word. When the defining word runs, `(does>)` patches the most recent definition — `does = ip; code = dodoes` — and exits; the thread after `(does>)` belongs to the created word, not the definer.

### JIT

The code field is the execution boundary made physical. Interpreted, specialized, or native — switching strategy for a word means writing one cell, and no caller ever changes. A hot does-word is JITted by compiling its thread and swapping `code`; `does` stays where the compiler can read it.

## Exceptions

Ash does not build `CATCH` / `THROW` on `setjmp` / `longjmp`. It builds them on the Forth return stack.

This works because of one property the inner interpreter must guarantee: **executing a colon definition never nests C stack frames.** `docol` and `exit` move `ip` and the Forth return stack; the C-level inner loop neither recurses nor unwinds. (The `EXECUTE` primitive must respect this too: dispatching a colon word adjusts `ip` and returns to the same loop.) A non-local return is therefore pure stack surgery on the data and return stacks — something Forth can express itself.

The C kernel contributes only stack-pointer access: `sp@ sp! rp@ rp!`. Everything else is `core.fs`, in the shape of the standard's own reference implementation:

```forth
variable handler

: catch  ( xt -- exception# | 0 )
    sp@ >r  handler @ >r  rp@ handler !
    execute
    r> handler !  r> drop  0 ;

: throw  ( n -- )
    ?dup if
        handler @ rp!  r> handler !
        r> swap >r  sp!  drop  r>
    then ;
```

`QUIT` empties the return stack (`rp!`) and re-enters the outer interpreter loop; `ABORT` empties the data stack and performs `QUIT`; an uncaught `THROW` falls back to `ABORT`'s behavior. The standard's rationale describes `CATCH`/`THROW` as a non-local return similar to C's `setjmp`/`longjmp` — Ash takes that comparison literally and then refuses the C mechanism, because exception semantics belong above the machine boundary. A small C kernel. The rest is Forth — including the exceptions.

## Primitives and the bootstrapping boundary

The C kernel contains only the primitive operations needed to bootstrap the Forth layer.

Words are not in C because Forth cannot express them — many of them could be defined in terms of one another. They are in C because they sit on the bootstrapping boundary Ash chose. The kernel splits them into two groups:

```text
machine primitives — the boundary to the machine
    stack manipulation     dup drop swap over rot >r r> r@ depth
    stack pointer access   sp@ sp! rp@ rp!
    arithmetic / logic     + - * / mod = < > 0= and or xor invert
    memory access          @ ! c@ c!
    I/O                    key emit type
    threaded dispatch      lit branch 0branch exit execute

bootstrap primitives — the minimum needed for Forth to define Forth
    dictionary creation    create here allot , find
    compilation            : ; immediate (does>)
    parsing / state        parse-name refill state >in base source-id
```

Everything above this boundary is derived Forth, written in `core.fs`:

```text
derived Forth
    2dup 2drop negate abs
    if else then
    begin until
    do loop
    constant variable does>
    catch throw abort quit
    ...
```

Control structures are **not** primitives: they are immediate words over `branch` / `0branch`. Exceptions are **not** C: they are return-stack surgery over `sp@ sp! rp@ rp!`. The project's ambition is to keep moving this boundary downward — to see how much of the C can be burned away, leaving ash.
