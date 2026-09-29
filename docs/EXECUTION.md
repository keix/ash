# Ash Execution Walkthrough

[DESIGN.md](DESIGN.md) says what the machinery is; this file traces
what it does, in call order, with the stacks drawn at each step.

## Boot

```text
main
 ├─ register_prims            C primitives into the dictionary
 ├─ load_file "forth/core.fs"
 │    ├─ push_source          file text becomes the active source
 │    ├─ interpret_source     the ordinary outer loop, nothing special
 │    └─ pop_source
 ├─ argv files                same path as core.fs, then exit
 └─ repl                      no argv: fgets → src[0] → interpret_source
```

One outer loop, many sources. By the time the prompt appears, the
Forth layer has already been interpreted through the same code path
the user is about to use.

## One line, token by token

`1 2 +` in interpret state:

```text
interpret_source
 ├─ next_token "1" ─ interpret_token
 │                    ├─ find_word ........ miss
 │                    └─ parse_number ..... push 1        ( 1 )
 ├─ next_token "2" ─ ...................... push 2        ( 1 2 )
 ├─ next_token "+" ─ interpret_token
 │                    ├─ find_word ........ hit
 │                    └─ run_xt(+)  ....... prim_add      ( 3 )
 └─ (stack checks between tokens)
```

The tokenizer only cuts; `interpret_token` holds all judgment; the
stack-misuse check runs between tokens.

## Entering threaded code: run_xt and the NULL sentinel

`: square dup * ;` already defined. Executing `square`:

```text
run_xt(square)
    ip = NULL                        plant the sentinel

    dispatch(square) → docol
        rpush ip                     rstack: [NULL]
        ip = square.body             [ dup | * | exit ]

    loop: w = *ip++, dispatch(w)
        dup   → prim_dup             ( 7 7 )
        *     → prim_mul             ( 49 )
        exit  → do_exit: ip = rpop   rstack: []   ip = NULL

    while (ip) fails → restore saved ip, return to C
```

The sentinel is not a special case anywhere: `docol` saves it like any
ip, and the outermost `exit` pops it back. The loop simply notices ip
became NULL.

## Nested calls never grow the C stack

`: quad square square ;`, executing `quad`:

```text
                                     rstack             C frames
run_xt(quad)
    docol(quad)                      [NULL]                 1
    w = square → docol               [NULL, &quad.body[1]]  1
        dup * exit → ip = rpop       [NULL]                 1
    w = square → docol               [NULL, &quad.body[1]]  1
        dup * exit → ip = rpop       [NULL]                 1
    w = exit → ip = rpop = NULL      []                     1
return
```

The C column never moves. That constant is the property CATCH/THROW
will rely on: discarding Forth continuations is pure return-stack
surgery, with no C unwinding to do.

The `execute` word keeps the same guarantee by NOT calling `run_xt`:
it performs one bare `dispatch`. A colon word just moves ip, and the
surrounding loop — already running — carries on inside the callee.

## Compiling a colon definition

`: square dup * ;` — call order, one token at a time:

```text
":"    prim_colon        parse "square"
                         dict_header (HIDDEN), code = docol
                         state = -1
"dup"  interpret_token   found, state≠0, not immediate → comma(dup-xt)
"*"    ...........................................  → comma(*-xt)
";"    immediate! → run_xt(;) → prim_semi
                         comma(exit-xt), unhide, state = 0
```

Result in memory:

```text
link | flags name_len "square" pad | does=0 | code=docol | dup * exit
                                              ▲ xt        ▲ body
```

State changed what interpretation means; the token stream never
noticed.

## Numbers under compilation: lit

`: five 5 ;` compiles an operand into the thread:

```text
compile:   comma(lit-xt)  comma(5)

thread:    [ lit | 5 | exit ]

run:       dispatch(lit) → prim_lit: push(*ip); ip++
                           ip steps OVER the operand
```

`lit`, `branch`, `0branch`, and `(s")` all follow this shape: the cell
after their xt is data, and the primitive moves ip past it.

## Control flow: compile-time patching

Compiling `: t if 111 then ;` — `if` and `then` are immediate, so they
run during compilation and talk to each other through the data stack:

```text
"if"    runs now:  comma(0branch)  push here  comma(0)
        thread: [ 0branch | ???? ]              ( hole-addr )
"111"   comma(lit) comma(111)
        thread: [ 0branch | ???? | lit | 111 ]  ( hole-addr )
"then"  runs now:  here swap !
        thread: [ 0branch | ──┐  | lit | 111 ]  (        )
                              └────────────────▶ here
";"     comma(exit)
```

At run time `0branch` pops the flag: zero jumps through the patched
cell, nonzero steps over it. The hole's address lived on the ordinary
data stack between `if` and `then` — that is the whole mechanism.

## create / does> in three phases

`: constant create , does> @ ;` then `42 constant answer` then
`answer`:

```text
phase 1 — compiling constant
    does>(immediate) compiles (does>):
    constant.body: [ create | , | (does>) | @ | exit ]

phase 2 — running "42 constant answer"
    docol(constant)              rstack: [NULL]  ip → create
    create    header "answer", code = docreate
    ,         answer.body[0] = 42
    (does>)   answer.does = ip        ── ip points at [ @ | exit ]
              answer.code = dodoes
              ip = rpop = NULL        ── exits constant: the rest of
                                         its thread belongs to answer

phase 3 — running "answer"
    dispatch → dodoes
        push &answer.body            ( addr-of-42 )
        rpush ip                     rstack: [NULL]
        ip = answer.does             the [ @ | exit ] inside constant
    @      → ( 42 )
    exit   → ip = rpop = NULL → done
```

The data field never moved — `,` filled it before `(does>)` patched —
so `>body` stays `xt + 1` for every word. One thread inside `constant`
is shared by every word `constant` defines.

## String literals: stepping over inline data

`: greet s" hi" type ;` — `s"` (immediate) lays the string into the
thread; `(s")` steps over it at run time:

```text
thread:  [ (s") | 2 | 'h' 'i' ~~pad~~ | type | exit ]

run:     dispatch((s")):
             len = *ip++              reads the 2
             push ip  push len        ( addr 2 )
             ip += cells(len)         lands on type
```

## Input sources nest and resume

`core.fs` loading, `evaluate` (future), and the REPL all share one
picture:

```text
            src[0] terminal  "1 2 evaluate-something 3 4"
                                         │ >in
push_source ─────────▶ src[1] string  "5 6"
                                        │ >in (its own)
   ... same interpret_source loop ...   exhausted
pop_source  ─────────▶ src[0] resumes exactly at its saved >in
```

Each source carries its own `>IN`; the tokenizer only ever reads the
active one. Nothing else in the system knows where text comes from.
