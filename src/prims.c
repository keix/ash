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

/* I/O */

static void
prim_dot (vm_t *vm, xt_t xt)
{
  (void)xt;
  static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char buf[8 * sizeof (cell_t) + 2];
  char *p = buf + sizeof buf;
  cell_t x = pop (vm);
  uintmax_t base = (uintmax_t)vm->base;
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
