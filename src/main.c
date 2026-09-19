#include <stdio.h>
#include <string.h>

#include "ash.h"

enum
{
  DSTACK_CELLS = 1024,
  RSTACK_CELLS = 1024,
  DICT_BYTES = 1 << 16,
  TIB_BYTES = 1024
};

static cell_t dstack[DSTACK_CELLS];
static cell_t rstack[RSTACK_CELLS];
static uint8_t dictionary[DICT_BYTES];
static char tib[TIB_BYTES];

int
main (void)
{
  vm_t vm = { 0 };

  vm.dsp = vm.dsp0 = dstack + DSTACK_CELLS;
  vm.rsp = vm.rsp0 = rstack + RSTACK_CELLS;
  vm.here = dictionary;
  vm.base = 10;

  register_prims (&vm);

  while (fgets (tib, sizeof tib, stdin))
    {
      input_source_t *src = active_source (&vm);
      src->buf = tib;
      src->len = (cell_t)strlen (tib);
      src->in = 0;
      src->source_id = 0;
      interpret_source (&vm);
      fputs (" ok\n", stdout);
      fflush (stdout);
    }
  return 0;
}
