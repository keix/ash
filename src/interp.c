#include <stdio.h>

#include "ash.h"

/* Convert tok in the current BASE. Accepts an optional leading '-';
   digits are 0-9 then a-z / A-Z up to the base. */
int
parse_number (vm_t *vm, const char *tok, size_t len, cell_t *out)
{
  uintmax_t base = (uintmax_t)vm->base;
  uintmax_t u = 0;
  int neg = 0;
  size_t i = 0;

  if (len > 0 && tok[0] == '-')
    {
      neg = 1;
      i = 1;
    }
  if (i >= len)
    return 0;
  for (; i < len; i++)
    {
      char c = tok[i];
      uintmax_t d;

      if (c >= '0' && c <= '9')
        d = (uintmax_t)(c - '0');
      else if (c >= 'a' && c <= 'z')
        d = (uintmax_t)(c - 'a') + 10;
      else if (c >= 'A' && c <= 'Z')
        d = (uintmax_t)(c - 'A') + 10;
      else
        return 0;
      if (d >= base)
        return 0;
      u = u * base + d;
    }
  *out = neg ? -(cell_t)u : (cell_t)u;
  return 1;
}

/* Poor man's ABORT until THROW exists in Forth: report, clear the
   data stack, leave compile state, discard the parse area. */
static void
error_undefined (vm_t *vm, const char *tok, size_t len)
{
  input_source_t *src = active_source (vm);

  fprintf (stderr, "undefined word: %.*s\n", (int)len, tok);
  vm->dsp = vm->dsp0;
  vm->state = 0;
  src->in = src->len;
}

void
interpret_token (vm_t *vm, const char *tok, size_t len)
{
  dict_entry_t *w = find_word (vm, tok, len);
  cell_t n;

  if (w)
    {
      if (vm->state && !(w->flags & F_IMMEDIATE))
        comma (vm, (cell_t)entry_xt (w));
      else
        execute (vm, entry_xt (w));
    }
  else if (parse_number (vm, tok, len, &n))
    {
      if (vm->state)
        {
          comma (vm, (cell_t)vm->xt_lit);
          comma (vm, n);
        }
      else
        push (vm, n);
    }
  else
    error_undefined (vm, tok, len);
}

void
interpret_source (vm_t *vm)
{
  const char *tok;
  size_t len;

  while ((tok = next_token (vm, &len)))
    interpret_token (vm, tok, len);
}
