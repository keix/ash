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
  if (fseek (f, 0, SEEK_END) != 0 || (n = ftell (f)) < 0
      || fseek (f, 0, SEEK_SET) != 0)
    {
      fclose (f);
      return NULL;
    }
  buf = malloc ((size_t)n + 1); /* +1: never malloc(0) */
  if (buf && fread (buf, 1, (size_t)n, f) != (size_t)n)
    {
      free (buf);
      buf = NULL;
    }
  fclose (f);
  *len = (cell_t)n;
  return buf;
}

/* Interpret a file line by line, the ANS input model: SOURCE is the
   current line and >IN moves within it. Definitions span lines via
   state; an abort discards only the offending line. */
static int
load_file (vm_t *vm, const char *path)
{
  cell_t len;
  char *buf = slurp (path, &len);
  cell_t pos = 0;

  if (!buf)
    return -1;
  push_source (vm, buf, 0, -1);
  while (pos < len)
    {
      input_source_t *src = active_source (vm);
      char *nl = memchr (buf + pos, '\n', (size_t)(len - pos));
      cell_t line_len = nl ? (cell_t)(nl - (buf + pos)) : len - pos;

      src->buf = buf + pos;
      src->len = line_len;
      src->in = 0;
      interpret_source (vm);
      pos += line_len + (nl ? 1 : 0);
    }
  pop_source (vm);
  free (buf);
  return 0;
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
main (int argc, char **argv)
{
  vm_t vm = { 0 };

  /* line-buffer stdout so its diagnostics and stderr's interleave in
     write order -- a terminal already does this; a pipe would not */
  setvbuf (stdout, NULL, _IOLBF, 0);

  vm.dsp = vm.dsp0 = dstack + DSTACK_CELLS;
  vm.rsp = vm.rsp0 = rstack + RSTACK_CELLS;
  vm.dsp_lim = dstack;
  vm.rsp_lim = rstack;
  vm.here = dictionary;
  vm.here_lim = dictionary + DICT_BYTES;
  vm.base = 10;

  register_prims (&vm);
  jit_register (&vm);
  load_file (&vm, CORE_FS);

  /* gforth-shaped invocation: interpret the named files, then exit */
  if (argc > 1)
    {
      for (int i = 1; i < argc; i++)
        if (load_file (&vm, argv[i]) != 0)
          {
            fprintf (stderr, "cannot open %s\n", argv[i]);
            return 1;
          }
      return 0;
    }

  /* the quit loop lives in forth when core.fs provided it; the C
     loop below remains only as the bootstrap fallback */
  {
    dict_entry_t *q = find_word (&vm, "(quit-loop)", 11);

    if (q)
      {
        if (isatty (STDIN_FILENO))
          fputs ("Ash, Copyright (C) 2026 KEI SAWAMURA\n"
                 "Ash is licensed under the MIT License.\n"
                 "Copying and modifying is encouraged and appreciated. "
                 "Type `bye' to exit\n",
                 stdout);
        run_xt (&vm, entry_xt (q));
        return 0;
      }
  }

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
