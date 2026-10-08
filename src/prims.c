#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ash.h"

static xt_t
defprim (vm_t *vm, const char *name, code_t code)
{
  xt_t xt = entry_xt (dict_header (vm, name, strlen (name)));
  *xt = (cell_t)code;
  return xt;
}

/* threaded dispatch */

static void
prim_lit (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, (cell_t)*vm->ip++);
}

/* (s"): runtime of string literals. The thread holds a length cell,
   then the bytes, padded to a cell boundary; step ip over them. */
static void
prim_do_squote (vm_t *vm, xt_t xt)
{
  cell_t len = (cell_t)*vm->ip++;
  size_t cells = ((size_t)len + sizeof (cell_t) - 1) / sizeof (cell_t);

  (void)xt;
  push (vm, (cell_t)vm->ip);
  push (vm, len);
  vm->ip += cells;
}

/* stack manipulation */

static void
prim_dup (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t a = pop (vm);
  push (vm, a);
  push (vm, a);
}

static void
prim_drop (vm_t *vm, xt_t xt)
{
  (void)xt;
  pop (vm);
}

static void
prim_swap (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm), a = pop (vm);
  push (vm, b);
  push (vm, a);
}

static void
prim_over (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm), a = pop (vm);
  push (vm, a);
  push (vm, b);
  push (vm, a);
}

static void
prim_depth (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, vm->dsp0 - vm->dsp);
}

static void
prim_rot (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t c = pop (vm), b = pop (vm), a = pop (vm);
  push (vm, b);
  push (vm, c);
  push (vm, a);
}

/* arithmetic */

static void
prim_add (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm);
  push (vm, pop (vm) + b);
}

static void
prim_sub (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm);
  push (vm, pop (vm) - b);
}

static void
prim_mul (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm);
  push (vm, pop (vm) * b);
}

static void
prim_div (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm);
  push (vm, pop (vm) / b);
}

static void
prim_mod (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm);
  push (vm, pop (vm) % b);
}

/* unsigned 64x64 -> 128 multiply, portable 32-bit halves. The cell
   is 64 bits by SPEC.md; no compiler 128-bit extension needed. */
static void
prim_um_star (vm_t *vm, xt_t xt)
{
  uintptr_t b = (uintptr_t)pop (vm);
  uintptr_t a = (uintptr_t)pop (vm);
  uintptr_t al = a & 0xffffffffu, ah = a >> 32;
  uintptr_t bl = b & 0xffffffffu, bh = b >> 32;
  uintptr_t ll = al * bl;
  uintptr_t mid = ah * bl + (ll >> 32); /* cannot overflow */
  uintptr_t mid2 = al * bh + (mid & 0xffffffffu);
  uintptr_t hi = ah * bh + (mid >> 32) + (mid2 >> 32);
  uintptr_t lo = (mid2 << 32) | (ll & 0xffffffffu);

  (void)xt;
  push (vm, (cell_t)lo);
  push (vm, (cell_t)hi);
}

/* 128 / 64 -> 64 remainder and quotient, restoring division.
   Quotient overflow (hi >= u) is an ambiguous condition. */
static void
prim_um_slash_mod (vm_t *vm, xt_t xt)
{
  uintptr_t u = (uintptr_t)pop (vm);
  uintptr_t hi = (uintptr_t)pop (vm);
  uintptr_t lo = (uintptr_t)pop (vm);
  uintptr_t q = 0, r = hi;
  int i;

  (void)xt;
  for (i = 0; i < 64; i++)
    {
      uintptr_t carry = r >> 63;

      r = (r << 1) | (lo >> 63);
      lo <<= 1;
      q <<= 1;
      if (carry || r >= u)
        {
          r -= u;
          q |= 1;
        }
    }
  push (vm, (cell_t)r);
  push (vm, (cell_t)q);
}

/* logic and shifts. rshift is logical, per ANS */

static void
prim_and (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm);
  push (vm, pop (vm) & b);
}

static void
prim_or (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm);
  push (vm, pop (vm) | b);
}

static void
prim_xor (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm);
  push (vm, pop (vm) ^ b);
}

static void
prim_invert (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, ~pop (vm));
}

static void
prim_lshift (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t u = pop (vm);
  push (vm, (cell_t)((uintptr_t)pop (vm) << u));
}

static void
prim_rshift (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t u = pop (vm);
  push (vm, (cell_t)((uintptr_t)pop (vm) >> u));
}

/* return stack */

static void
prim_tor (vm_t *vm, xt_t xt)
{
  (void)xt;
  rpush (vm, pop (vm));
}

static void
prim_fromr (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, rpop (vm));
}

static void
prim_rfetch (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, *vm->rsp);
}

/* stack pointer access: what CATCH/THROW build their surgery on */

static void
prim_sp_fetch (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t v = (cell_t)vm->dsp;
  push (vm, v);
}

static void
prim_sp_store (vm_t *vm, xt_t xt)
{
  (void)xt;
  vm->dsp = (cell_t *)pop (vm);
}

static void
prim_rp_fetch (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, (cell_t)vm->rsp);
}

static void
prim_rp_store (vm_t *vm, xt_t xt)
{
  (void)xt;
  vm->rsp = (cell_t *)pop (vm);
}

/* quit and the uncaught-throw landing pad. Setting ip to NULL makes
   run_xt's loop end after this primitive returns: the clean way back
   to C from any nesting depth, because Forth control flow never
   lives on the C stack. */

static void
prim_quit (vm_t *vm, xt_t xt)
{
  input_source_t *src = active_source (vm);

  (void)xt;
  src->in = src->len;
  vm->state = 0;
  vm->rsp = vm->rsp0;
  vm->ip = NULL;
}

static void
prim_do_abort (vm_t *vm, xt_t xt)
{
  (void)xt;
  abort_line (vm);
  vm->rsp = vm->rsp0;
  vm->ip = NULL;
}

/* comparison: Forth flags, -1 true and 0 false */

static void
prim_eq (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm);
  push (vm, pop (vm) == b ? -1 : 0);
}

static void
prim_lt (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm);
  push (vm, pop (vm) < b ? -1 : 0);
}

static void
prim_gt (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm);
  push (vm, pop (vm) > b ? -1 : 0);
}

static void
prim_zeq (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, pop (vm) == 0 ? -1 : 0);
}

/* memory and data space */

static void
prim_fetch (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, *(cell_t *)pop (vm));
}

static void
prim_store (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t *addr = (cell_t *)pop (vm);
  *addr = pop (vm);
}

static void
prim_here (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, (cell_t)vm->here);
}

static void
prim_comma (vm_t *vm, xt_t xt)
{
  (void)xt;
  comma (vm, pop (vm));
}

static void
prim_cfetch (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, *(uint8_t *)pop (vm));
}

static void
prim_cstore (vm_t *vm, xt_t xt)
{
  (void)xt;
  uint8_t *addr = (uint8_t *)pop (vm);
  *addr = (uint8_t)pop (vm);
}

static void
prim_ccomma (vm_t *vm, xt_t xt)
{
  (void)xt;
  *(uint8_t *)allot (vm, 1) = (uint8_t)pop (vm);
}

/* branching: the cell after the branch xt is an absolute target */

/* A taken backward branch is a loop iteration: the JIT counts it, and
   at its threshold may run the rest of this activation natively,
   leaving ip wherever the word's exit put it. */
static void
prim_branch (vm_t *vm, xt_t xt)
{
  xt_t *t = (xt_t *)*vm->ip;

  (void)xt;
  if (t <= vm->ip && jit_backedge (vm, t))
    return;
  vm->ip = t;
}

static void
prim_0branch (vm_t *vm, xt_t xt)
{
  xt_t *t = (xt_t *)*vm->ip;

  (void)xt;
  if (pop (vm) != 0)
    {
      vm->ip++;
      return;
    }
  if (t <= vm->ip && jit_backedge (vm, t))
    return;
  vm->ip = t;
}

/* interpreter surface */

/* One bare dispatch, not run_xt: a colon word just moves ip and the
   surrounding loop runs its body, so C stack frames never nest. */
static void
prim_execute (vm_t *vm, xt_t xt)
{
  (void)xt;
  dispatch (vm, (xt_t)pop (vm));
}

/* ans find: c-addr -- c-addr 0 | xt 1 | xt -1 */
static void
prim_find (vm_t *vm, xt_t xt)
{
  cell_t a = pop (vm);
  const uint8_t *s = (const uint8_t *)a;
  dict_entry_t *w = find_word (vm, (const char *)s + 1, s[0]);

  (void)xt;
  if (!w)
    {
      push (vm, a);
      push (vm, 0);
    }
  else
    {
      push (vm, (cell_t)entry_xt (w));
      push (vm, (w->flags & F_IMMEDIATE) ? 1 : -1);
    }
}

static void
prim_state (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, (cell_t)&vm->state);
}

static void
prim_latest (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, (cell_t)vm->latest);
}

static void
prim_base (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, (cell_t)&vm->base);
}

static void
prim_to_in (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, (cell_t)&active_source (vm)->in);
}

/* parse: the delimiter-argument sibling of next_token. No leading
   skip; one trailing delimiter consumed. Zero judgment. */
static void
prim_parse (vm_t *vm, xt_t xt)
{
  input_source_t *src = active_source (vm);
  char delim = (char)pop (vm);
  const char *start = src->buf + src->in;
  cell_t n = 0;

  (void)xt;
  while (src->in < src->len && src->buf[src->in] != delim)
    {
      src->in++;
      n++;
    }
  if (src->in < src->len)
    src->in++;
  push (vm, (cell_t)start);
  push (vm, n);
}

static void
prim_parse_name (vm_t *vm, xt_t xt)
{
  size_t len = 0;
  const char *name = next_token (vm, &len);

  (void)xt;
  push (vm, (cell_t)name);
  push (vm, name ? (cell_t)len : 0);
}

static void
prim_align (vm_t *vm, xt_t xt)
{
  (void)xt;
  align_here (vm);
}

static void
prim_source (vm_t *vm, xt_t xt)
{
  input_source_t *src = active_source (vm);

  (void)xt;
  push (vm, (cell_t)src->buf);
  push (vm, src->len);
}

static void
prim_push_source (vm_t *vm, xt_t xt)
{
  cell_t id = pop (vm);
  cell_t len = pop (vm);
  const char *buf = (const char *)pop (vm);

  (void)xt;
  if (vm->src_depth + 1 >= SOURCE_DEPTH)
    {
      fprintf (stderr, "source stack overflow\n");
      abort_line (vm);
      vm->rsp = vm->rsp0;
      vm->ip = NULL;
      return;
    }
  push_source (vm, buf, len, id);
}

static void
prim_pop_source (vm_t *vm, xt_t xt)
{
  (void)xt;
  pop_source (vm);
}

static void
prim_allot (vm_t *vm, xt_t xt)
{
  cell_t n = pop (vm);

  (void)xt;
  if (n < 0)
    vm->here += n; /* ans allows releasing space */
  else
    allot (vm, (size_t)n);
}

/* I/O */

static void
prim_emit (vm_t *vm, xt_t xt)
{
  (void)xt;
  fputc ((int)(uint8_t)pop (vm), stdout);
}

static void
prim_key (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, getchar ());
}

/* accept: read up to n1 chars into the buffer, stopping at newline;
   return the count read */
static void
prim_accept (vm_t *vm, xt_t xt)
{
  cell_t n1 = pop (vm);
  char *addr = (char *)pop (vm);
  cell_t n2 = 0;
  int c;

  (void)xt;
  while (n2 < n1 && (c = getchar ()) != EOF && c != '\n')
    addr[n2++] = (char)c;
  push (vm, n2);
}

static void
prim_bye (vm_t *vm, xt_t xt)
{
  (void)vm;
  (void)xt;
  exit (0);
}

/* defining and parsing words */

static void
prim_colon (vm_t *vm, xt_t xt)
{
  size_t len;
  const char *name = next_token (vm, &len);
  dict_entry_t *w;

  (void)xt;
  if (!name)
    {
      fprintf (stderr, ": needs a name\n");
      return;
    }
  w = dict_header (vm, name, len);
  if (!w)
    {
      input_source_t *src = active_source (vm);

      fprintf (stderr, ": bad name\n");
      src->in = src->len;
      return;
    }
  w->flags |= F_HIDDEN;
  *entry_xt (w) = (cell_t)docol;
  vm->state = -1;
}

static void
prim_create (vm_t *vm, xt_t xt)
{
  size_t len;
  const char *name = next_token (vm, &len);
  dict_entry_t *w;

  (void)xt;
  if (!name)
    {
      fprintf (stderr, "create needs a name\n");
      return;
    }
  w = dict_header (vm, name, len);
  if (!w)
    {
      input_source_t *src = active_source (vm);

      fprintf (stderr, "create: bad name\n");
      src->in = src->len;
      return;
    }
  *entry_xt (w) = (cell_t)docreate;
}

/* (does>): patch the latest definition so it pushes its data field
   and runs the thread after this word, then exit -- the rest of the
   defining word's body belongs to the child. */
static void
prim_paren_does (vm_t *vm, xt_t xt)
{
  xt_t w = entry_xt (vm->latest);

  (void)xt;
  w[-1] = (cell_t)vm->ip;
  *w = (cell_t)dodoes;
  vm->ip = (xt_t *)rpop (vm);
}

static void
prim_semi (vm_t *vm, xt_t xt)
{
  (void)xt;
  comma (vm, (cell_t)vm->xt_exit);
  vm->latest->flags &= ~F_HIDDEN;
  vm->state = 0;
  if (vm->jit_on > 0)
    jit_xt (vm, entry_xt (vm->latest));
}

static void
prim_immediate (vm_t *vm, xt_t xt)
{
  (void)xt;
  vm->latest->flags |= F_IMMEDIATE;
}

static dict_entry_t *
parse_word (vm_t *vm, const char *who)
{
  size_t len;
  const char *name = next_token (vm, &len);
  dict_entry_t *w = name ? find_word (vm, name, len) : NULL;

  if (!w)
    fprintf (stderr, "%s: word not found: %.*s\n", who, name ? (int)len : 0,
             name ? name : "");
  return w;
}

static void
prim_tick (vm_t *vm, xt_t xt)
{
  dict_entry_t *w = parse_word (vm, "'");

  (void)xt;
  if (w)
    push (vm, (cell_t)entry_xt (w));
}

/* ['] : immediate; compile the parsed word's xt as a literal */
static void
prim_bracket_tick (vm_t *vm, xt_t xt)
{
  dict_entry_t *w = parse_word (vm, "[']");

  (void)xt;
  if (w)
    {
      comma (vm, (cell_t)vm->xt_lit);
      comma (vm, (cell_t)entry_xt (w));
    }
}

static void
prim_backslash (vm_t *vm, xt_t xt)
{
  input_source_t *src = active_source (vm);

  (void)xt;
  while (src->in < src->len && src->buf[src->in] != '\n')
    src->in++;
}

void
register_prims (vm_t *vm)
{
  defprim (vm, "dup", prim_dup);
  defprim (vm, "drop", prim_drop);
  defprim (vm, "swap", prim_swap);
  defprim (vm, "over", prim_over);
  defprim (vm, "rot", prim_rot);
  defprim (vm, "depth", prim_depth);

  defprim (vm, "+", prim_add);
  defprim (vm, "-", prim_sub);
  defprim (vm, "*", prim_mul);
  defprim (vm, "/", prim_div);
  defprim (vm, "mod", prim_mod);
  defprim (vm, "um*", prim_um_star);
  defprim (vm, "um/mod", prim_um_slash_mod);

  defprim (vm, "sp@", prim_sp_fetch);
  defprim (vm, "sp!", prim_sp_store);
  defprim (vm, "rp@", prim_rp_fetch);
  defprim (vm, "rp!", prim_rp_store);
  defprim (vm, "quit", prim_quit);
  defprim (vm, "(abort)", prim_do_abort);

  defprim (vm, ">r", prim_tor);
  defprim (vm, "r>", prim_fromr);
  defprim (vm, "r@", prim_rfetch);

  defprim (vm, "and", prim_and);
  defprim (vm, "or", prim_or);
  defprim (vm, "xor", prim_xor);
  defprim (vm, "invert", prim_invert);
  defprim (vm, "lshift", prim_lshift);
  defprim (vm, "rshift", prim_rshift);

  defprim (vm, "=", prim_eq);
  defprim (vm, "<", prim_lt);
  defprim (vm, ">", prim_gt);
  defprim (vm, "0=", prim_zeq);

  defprim (vm, "@", prim_fetch);
  defprim (vm, "!", prim_store);
  defprim (vm, "c@", prim_cfetch);
  defprim (vm, "c!", prim_cstore);
  defprim (vm, "here", prim_here);
  defprim (vm, ",", prim_comma);
  defprim (vm, "c,", prim_ccomma);
  defprim (vm, "allot", prim_allot);

  defprim (vm, "branch", prim_branch);
  defprim (vm, "0branch", prim_0branch);

  defprim (vm, "execute", prim_execute);
  defprim (vm, "find", prim_find);
  defprim (vm, "state", prim_state);
  defprim (vm, "latest", prim_latest);
  defprim (vm, "base", prim_base);
  defprim (vm, ">in", prim_to_in);
  defprim (vm, "parse", prim_parse);
  defprim (vm, "parse-name", prim_parse_name);
  defprim (vm, "source", prim_source);
  defprim (vm, "(push-source)", prim_push_source);
  defprim (vm, "(pop-source)", prim_pop_source);
  defprim (vm, "align", prim_align);
  defprim (vm, "(s\")", prim_do_squote);

  defprim (vm, "'", prim_tick);
  defprim (vm, "[']", prim_bracket_tick);
  vm->latest->flags |= F_IMMEDIATE;

  defprim (vm, "emit", prim_emit);
  defprim (vm, "key", prim_key);
  defprim (vm, "accept", prim_accept);
  defprim (vm, "bye", prim_bye);

  vm->xt_lit = defprim (vm, "lit", prim_lit);
  vm->xt_exit = defprim (vm, "exit", do_exit);

  defprim (vm, "create", prim_create);
  defprim (vm, "(does>)", prim_paren_does);

  defprim (vm, ":", prim_colon);
  defprim (vm, ";", prim_semi);
  vm->latest->flags |= F_IMMEDIATE;
  defprim (vm, "immediate", prim_immediate);
  defprim (vm, "\\", prim_backslash);
  vm->latest->flags |= F_IMMEDIATE;
}
