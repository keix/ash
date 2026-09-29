/* inner interpreter: threading, nesting, the no-C-frames property */
#include <assert.h>
#include <stdio.h>

#include "ash.h"

static uint8_t space[4096];
static cell_t dstack[64];
static cell_t rstack[64];

static void
prim_add (vm_t *vm, xt_t xt)
{
  (void)xt;
  cell_t b = pop (vm), a = pop (vm);
  push (vm, a + b);
}

static void
prim_fetch (vm_t *vm, xt_t xt)
{
  (void)xt;
  push (vm, *(cell_t *)pop (vm));
}

static xt_t
defprim (vm_t *vm, const char *name, size_t len, code_t code)
{
  xt_t xt = entry_xt (dict_header (vm, name, len));
  *xt = (cell_t)code;
  return xt;
}

int
main (void)
{
  vm_t vm = { 0 };
  vm.here = space;
  vm.dsp = vm.dsp0 = dstack + 64;
  vm.rsp = vm.rsp0 = rstack + 64;

  xt_t xt_plus = defprim (&vm, "+", 1, prim_add);
  xt_t xt_exit = defprim (&vm, "exit", 4, do_exit);

  /* a primitive through find -> xt -> code field */
  push (&vm, 1);
  push (&vm, 2);
  dict_entry_t *w = find_word (&vm, "+", 1);
  assert (w != NULL);
  run_xt (&vm, entry_xt (w));
  assert (pop (&vm) == 3);

  /* colon word: sum3 = [ + + exit ] */
  xt_t xt_sum3 = entry_xt (dict_header (&vm, "sum3", 4));
  *xt_sum3 = (cell_t)docol;
  comma (&vm, (cell_t)xt_plus);
  comma (&vm, (cell_t)xt_plus);
  comma (&vm, (cell_t)xt_exit);

  push (&vm, 1);
  push (&vm, 2);
  push (&vm, 3);
  run_xt (&vm, xt_sum3);
  assert (pop (&vm) == 6);

  /* nested colon words unwind through the NULL-ip sentinel */
  xt_t xt_nest = entry_xt (dict_header (&vm, "twice", 5));
  *xt_nest = (cell_t)docol;
  comma (&vm, (cell_t)xt_sum3);
  comma (&vm, (cell_t)xt_sum3);
  comma (&vm, (cell_t)xt_exit);

  push (&vm, 1);
  push (&vm, 2);
  push (&vm, 3);
  push (&vm, 4);
  push (&vm, 5);
  run_xt (&vm, xt_nest);
  assert (pop (&vm) == 15);

  /* both stacks fully unwound, ip back to NULL */
  assert (vm.dsp == vm.dsp0);
  assert (vm.rsp == vm.rsp0);
  assert (vm.ip == NULL);

  /* docreate pushes the data field */
  xt_t xt_var = entry_xt (dict_header (&vm, "v", 1));
  *xt_var = (cell_t)docreate;
  comma (&vm, 42);
  run_xt (&vm, xt_var);
  assert (*(cell_t *)pop (&vm) == 42);

  /* dodoes pushes the data field, then runs the thread at xt[-1] */
  xt_t xt_fetch = defprim (&vm, "f", 1, prim_fetch);
  xt_t xt_con = entry_xt (dict_header (&vm, "c", 1));
  *xt_con = (cell_t)dodoes;
  comma (&vm, 42);              /* data field: one cell */
  xt_con[-1] = (cell_t)vm.here; /* does thread follows */
  comma (&vm, (cell_t)xt_fetch);
  comma (&vm, (cell_t)xt_exit);
  run_xt (&vm, xt_con);
  assert (pop (&vm) == 42);
  assert (vm.dsp == vm.dsp0 && vm.rsp == vm.rsp0);

  puts ("test_exec: ok");
  return 0;
}
