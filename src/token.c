#include "ash.h"

input_source_t *
active_source (vm_t *vm)
{
  return &vm->src[vm->src_depth];
}

void
push_source (vm_t *vm, const char *buf, cell_t len, cell_t source_id)
{
  input_source_t *src = &vm->src[++vm->src_depth];
  src->buf = buf;
  src->len = len;
  src->in = 0;
  src->source_id = source_id;
  src->fbuf = NULL;
  src->flen = 0;
  src->fpos = 0;
}

void
pop_source (vm_t *vm)
{
  vm->src_depth--;
}

/* Advance a file source to its next line: SOURCE becomes that line,
   >IN resets. Returns 0 when the whole text is consumed. This is the
   refill of a file, shared by the C boot loader and the Forth refill. */
int
refill_line (vm_t *vm)
{
  input_source_t *src = active_source (vm);
  const char *all = src->fbuf;
  cell_t start = src->fpos;
  cell_t i = start;

  if (!all || start >= src->flen)
    return 0;
  while (i < src->flen && all[i] != '\n')
    i++;
  src->buf = all + start;
  src->len = i - start;
  src->in = 0;
  src->fpos = i < src->flen ? i + 1 : i;
  return 1;
}

/* Cut one whitespace-delimited token from the active input source,
   starting at its >IN. Returns a pointer into the source buffer
   (no copy). NULL when the source is exhausted. One trailing
   delimiter is consumed, so >IN lands just past it -- the convention
   parsing words like s" rely on. */
const char *
next_token (vm_t *vm, size_t *len)
{
  input_source_t *src = active_source (vm);
  const char *buf = src->buf;
  cell_t i = src->in;
  cell_t start;

  while (i < src->len && (unsigned char)buf[i] <= ' ')
    i++;
  if (i >= src->len)
    {
      src->in = i;
      return NULL;
    }
  start = i;
  while (i < src->len && (unsigned char)buf[i] > ' ')
    i++;
  src->in = i < src->len ? i + 1 : i;
  *len = (size_t)(i - start);
  return buf + start;
}
