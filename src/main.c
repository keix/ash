#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ash.h"

#define CORE_FS "forth/core.fs"

/* Read a whole file; the buffer stays alive for the words it defines
   only while interpreting, so interpret it before freeing. */
static char *
slurp (const char *path, cell_t *len)
{
  FILE *f = fopen (path, "rb");
  char *buf;
  long n;

  if (!f)
    return NULL;
  fseek (f, 0, SEEK_END);
  n = ftell (f);
  rewind (f);
  buf = malloc ((size_t)n);
  if (buf && fread (buf, 1, (size_t)n, f) != (size_t)n)
    {
      free (buf);
      buf = NULL;
    }
  fclose (f);
  *len = (cell_t)n;
  return buf;
}

static void
load_file (vm_t *vm, const char *path)
{
  cell_t len;
  char *buf = slurp (path, &len);

  if (!buf)
    return;
  push_source (vm, buf, len, -1);
  interpret_source (vm);
  pop_source (vm);
  free (buf);
}

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
  vm.dsp_lim = dstack;
  vm.rsp_lim = rstack;
  vm.here = dictionary;
  vm.here_lim = dictionary + DICT_BYTES;
  vm.base = 10;

  register_prims (&vm);
  load_file (&vm, CORE_FS);

  if (isatty (STDIN_FILENO))
    fputs ("Ash, Copyright (C) 2026 KEI SAWAMURA\n"
           "Ash is licensed under the MIT License.\n"
           "Copying and modifying is encouraged and appreciated. "
           "Type `bye' to exit\n",
           stdout);

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
