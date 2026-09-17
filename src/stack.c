#include "ash.h"

void
push (vm_t *vm, cell_t x)
{
  *--vm->dsp = x;
}

cell_t
pop (vm_t *vm)
{
  return *vm->dsp++;
}

void
rpush (vm_t *vm, cell_t x)
{
  *--vm->rsp = x;
}

cell_t
rpop (vm_t *vm)
{
  return *vm->rsp++;
}
