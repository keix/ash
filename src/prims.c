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
  defprim (vm, "exit", do_exit);
  defprim (vm, "bye", prim_bye);

  vm->xt_lit = defprim (vm, "lit", prim_lit);
}
