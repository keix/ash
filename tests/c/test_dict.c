/* dictionary layout invariants */
#include <assert.h>
#include <stdio.h>

#include "ash.h"

static uint8_t space[4096];

int
main (void)
{
  vm_t vm = { 0 };
  vm.here = space;

  dict_entry_t *dup_w = dict_header (&vm, "dup", 3);
  xt_t dup_xt = entry_xt (dup_w);
  *dup_xt = 0x1111; /* fake code */

  dict_entry_t *swap_w = dict_header (&vm, "swap", 4);
  xt_t swap_xt = entry_xt (swap_w);
  *swap_xt = 0x2222;

  /* layout invariants */
  assert (((uintptr_t)dup_xt % sizeof (cell_t)) == 0);
  assert (dup_xt[-1] == 0);                              /* does cell */
  assert ((uint8_t *)(dup_xt + 1) <= (uint8_t *)swap_w); /* body first */

  /* xt derived from header must equal xt at build time */
  assert (entry_xt (vm.latest) == swap_xt);

  /* lookup: hit, case-insensitivity, miss, link order */
  assert (find_word (&vm, "dup", 3) == dup_w);
  assert (find_word (&vm, "DUP", 3) == dup_w);
  assert (find_word (&vm, "swap", 4) == swap_w);
  assert (find_word (&vm, "nope", 4) == NULL);
  assert (swap_w->link == dup_w);

  /* hidden words are invisible */
  dup_w->flags |= F_HIDDEN;
  assert (find_word (&vm, "dup", 3) == NULL);
  dup_w->flags &= ~F_HIDDEN;

  /* redefinition shadows the old word */
  dict_entry_t *dup2 = dict_header (&vm, "dup", 3);
  assert (find_word (&vm, "dup", 3) == dup2);

  /* name-length boundary: 0 and >255 are rejected */
  assert (dict_header (&vm, "", 0) == NULL);
  assert (dict_header (&vm, "x", 256) == NULL);

  /* code fields kept their values */
  assert (*dup_xt == 0x1111 && *swap_xt == 0x2222);

  puts ("test_dict: ok");
  return 0;
}
