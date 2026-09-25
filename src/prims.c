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

/* branching: the cell after the branch xt is an absolute target */

static void
prim_branch (vm_t *vm, xt_t xt)
{
  (void)xt;
  vm->ip = (xt_t *)*vm->ip;
}

static void
prim_0branch (vm_t *vm, xt_t xt)
{
  (void)xt;
  if (pop (vm) == 0)
    vm->ip = (xt_t *)*vm->ip;
  else
    vm->ip++;
}

/* interpreter surface */

/* Dispatch the code field once, without calling the C execute(): a
   colon word just moves ip and the surrounding loop runs its body, so
   C stack frames never nest. */
static void
prim_execute (vm_t *vm, xt_t xt)
{
  xt_t x = (xt_t)pop (vm);

  (void)xt;
  (*(code_t *)x) (vm, x);
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
prim_to_in (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, (cell_t)&active_source (vm)->in);
}

/* I/O */

static void
prim_dot (vm_t *vm, xt_t xt)
{
  (void)xt;
  static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char buf[8 * sizeof (cell_t) + 2];
  char *p = buf + sizeof buf;
  cell_t x = pop (vm);
  uintmax_t base = (vm->base < 2 || vm->base > 36) ? 10 : (uintmax_t)vm->base;
  uintmax_t u = (uintmax_t)x;

  if (x < 0)
    u = -u;
  *--p = ' ';
  do
    {
      *--p = digits[u % base];
      u /= base;
    }
  while (u);
  if (x < 0)
    *--p = '-';
  fwrite (p, 1, (size_t)(buf + sizeof buf - p), stdout);
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
prim_semi (vm_t *vm, xt_t xt)
{
  (void)xt;
  comma (vm, (cell_t)vm->xt_exit);
  vm->latest->flags &= ~F_HIDDEN;
  vm->state = 0;
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

  defprim (vm, "+", prim_add);
  defprim (vm, "-", prim_sub);
  defprim (vm, "*", prim_mul);
  defprim (vm, "/", prim_div);
  defprim (vm, "mod", prim_mod);

  defprim (vm, ">r", prim_tor);
  defprim (vm, "r>", prim_fromr);
  defprim (vm, "r@", prim_rfetch);

  defprim (vm, "=", prim_eq);
  defprim (vm, "<", prim_lt);
  defprim (vm, ">", prim_gt);
  defprim (vm, "0=", prim_zeq);

  defprim (vm, "@", prim_fetch);
  defprim (vm, "!", prim_store);
  defprim (vm, "here", prim_here);
  defprim (vm, ",", prim_comma);

  defprim (vm, "branch", prim_branch);
  defprim (vm, "0branch", prim_0branch);

  defprim (vm, "execute", prim_execute);
  defprim (vm, "find", prim_find);
  defprim (vm, "state", prim_state);
  defprim (vm, ">in", prim_to_in);

  defprim (vm, "'", prim_tick);
  defprim (vm, "[']", prim_bracket_tick);
  vm->latest->flags |= F_IMMEDIATE;

  defprim (vm, ".", prim_dot);
  defprim (vm, "bye", prim_bye);

  vm->xt_lit = defprim (vm, "lit", prim_lit);
  vm->xt_exit = defprim (vm, "exit", do_exit);

  defprim (vm, ":", prim_colon);
  defprim (vm, ";", prim_semi);
  vm->latest->flags |= F_IMMEDIATE;
  defprim (vm, "immediate", prim_immediate);
  defprim (vm, "\\", prim_backslash);
  vm->latest->flags |= F_IMMEDIATE;
}
