#include "ash.h"

/* The inner interpreter never nests C stack frames: docol and do_exit
   only move ip and the Forth return stack, and execute() drives a
   single dispatch loop. A NULL ip is the sentinel for "return to C":
   execute() plants it, docol saves it like any other ip, and the
   outermost exit pops it back, ending the loop. */

void docol(vm_t *vm, xt_t xt)
{
    rpush(vm, (cell_t)vm->ip);
    vm->ip = (xt_t *)(xt + 1);
}

void do_exit(vm_t *vm, xt_t xt)
{
    (void)xt;
    vm->ip = (xt_t *)rpop(vm);
}

void execute(vm_t *vm, xt_t xt)
{
    xt_t *saved = vm->ip;

    vm->ip = NULL;
    (*(code_t *)xt)(vm, xt);
    while (vm->ip) {
        xt_t w = *vm->ip++;
        (*(code_t *)w)(vm, w);
    }
    vm->ip = saved;
}
