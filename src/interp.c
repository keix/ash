#include <stdio.h>

#include "ash.h"

/* Convert tok in the current BASE, honoring the Forth-2012 prefixes
   # (decimal), $ (hex), % (binary), and 'c' character literals. An
   optional '-' follows the prefix; digits are 0-9 then a-z / A-Z up
   to the base. Keep in sync with (number) in core.fs. */
int
parse_number (vm_t *vm, const char *tok, size_t len, cell_t *out)
{
  cell_t b = vm->base;
  uintmax_t base;
  uintmax_t u = 0;
  int neg = 0;
  size_t i = 0;

  if (len == 3 && tok[0] == '\'' && tok[2] == '\'')
    {
      *out = (uint8_t)tok[1];
      return 1;
    }
  if (len > 0 && tok[0] == '#')
    {
      b = 10;
      i = 1;
    }
  else if (len > 0 && tok[0] == '$')
    {
      b = 16;
      i = 1;
    }
  else if (len > 0 && tok[0] == '%')
    {
      b = 2;
      i = 1;
    }

  /* BASE is Forth-writable; out of range must not become C UB */
  if (b < 2 || b > 36)
    return 0;
  base = (uintmax_t)b;

  if (i < len && tok[i] == '-')
    {
      neg = 1;
      i++;
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

/* Poor man's ABORT for kernel-level errors: clear the data stack,
   leave compile state, discard the parse area. The Forth-level
   (abort) builds on this. */
void
abort_line (vm_t *vm)
{
  input_source_t *src = active_source (vm);

  vm->dsp = vm->dsp0;
  vm->state = 0;
  src->in = src->len;
}

static void
error_undefined (vm_t *vm, const char *tok, size_t len)
{
  fprintf (stderr, "undefined word: %.*s\n", (int)len, tok);
  abort_line (vm);
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
        run_xt (vm, entry_xt (w));
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

/* Stack misuse is detected here, between tokens: after the fact, but
   before the damage compounds. The inner loop stays uninstrumented. */
void
interpret_source (vm_t *vm)
{
  const char *tok;
  size_t len;

  while ((tok = next_token (vm, &len)))
    {
      interpret_token (vm, tok, len);
      if (vm->dsp > vm->dsp0 || vm->rsp > vm->rsp0)
        {
          fprintf (stderr, "stack underflow\n");
          vm->rsp = vm->rsp0;
          abort_line (vm);
        }
      else if (vm->dsp_lim && vm->dsp < vm->dsp_lim)
        {
          fprintf (stderr, "stack overflow\n");
          abort_line (vm);
        }
    }
}
