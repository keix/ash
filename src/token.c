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
}

void
pop_source (vm_t *vm)
{
  vm->src_depth--;
}

/* Cut one whitespace-delimited token from the active input source,
   starting at its >IN. Returns a pointer into the source buffer
   (no copy). NULL when the source is exhausted. */
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
  src->in = i;
  *len = (size_t)(i - start);
  return buf + start;
}
