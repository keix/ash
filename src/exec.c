#include "ash.h"

/* The inner interpreter never nests C stack frames: docol and do_exit
   only move ip and the Forth return stack, and execute() drives a
   single dispatch loop. A NULL ip is the sentinel for "return to C":
   execute() plants it, docol saves it like any other ip, and the
   outermost exit pops it back, ending the loop. */

void
docol (vm_t *vm, xt_t xt)
{
  rpush (vm, (cell_t)vm->ip);
  vm->ip = (xt_t *)(xt + 1);
}

void
do_exit (vm_t *vm, xt_t xt)
{
  (void)xt;
  vm->ip = (xt_t *)rpop (vm);
}

void
docreate (vm_t *vm, xt_t xt)
{
  push (vm, (cell_t)(xt + 1));
}

/* Push the data-field address, then run the DOES> thread at xt[-1]. */
void
dodoes (vm_t *vm, xt_t xt)
{
  push (vm, (cell_t)(xt + 1));
  rpush (vm, (cell_t)vm->ip);
  vm->ip = (xt_t *)xt[-1];
}

/* Run one xt to completion: the entry from C into threaded code.
   Calling this from inside running Forth would nest a C frame -- the
   EXECUTE primitive uses a bare dispatch instead. */
void
run_xt (vm_t *vm, xt_t xt)
{
  xt_t *saved = vm->ip;

  vm->ip = NULL;
  dispatch (vm, xt);
  while (vm->ip)
    dispatch (vm, *vm->ip++);
  vm->ip = saved;
}
