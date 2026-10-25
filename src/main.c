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

/* The ANS file input model: the whole text is one source, SOURCE is
   its current line, and refill advances across lines -- so a parsing
   word (REFILL, [IF]) can span lines. Once core.fs defines the Forth
   text interpreter, the file runs through (load-file); the C
   interpret_source loop remains only to bootstrap core.fs itself. */
static int
load_file (vm_t *vm, const char *path)
{
  cell_t len;
  char *buf = slurp (path, &len);
  input_source_t *src;
  dict_entry_t *fw = find_word (vm, "(load-file)", 11);

  if (!buf)
    return -1;
  push_source (vm, "", 0, 1); /* file source: id > 0 */
  src = active_source (vm);
  src->fbuf = buf;
  src->flen = len;
  src->fpos = 0;
  if (fw)
    run_xt (vm, entry_xt (fw));
  else
    while (refill_line (vm))
      interpret_source (vm);
  pop_source (vm);
  free (buf);
  return 0;
}

enum
{
  DSTACK_CELLS = 1024,
  RSTACK_CELLS = 1024,
  DICT_BYTES = 1 << 16,
  TIB_BYTES = 1024,
  DSTACK_SLACK = 64 /* past each end: overflow and underflow are caught
                       between tokens, so a token's overshoot must
                       stay in the array */
};

static cell_t dstack[DSTACK_SLACK + DSTACK_CELLS + DSTACK_SLACK];
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

  vm.dsp = vm.dsp0 = dstack + DSTACK_SLACK + DSTACK_CELLS;
  vm.rsp = vm.rsp0 = rstack + RSTACK_CELLS;
  vm.dsp_lim = dstack + DSTACK_SLACK;
  vm.rsp_lim = rstack;
  vm.here = dictionary;
  vm.here_lim = dictionary + DICT_BYTES;
  vm.base = 10;

  register_prims (&vm);
  jit_register (&vm);
  /* without core.fs the kernel has no . and no error reporting: a
     silent half-forth is worse than refusing to start */
  if (load_file (&vm, CORE_FS) != 0)
    {
      fprintf (stderr, "cannot open %s: run ash from the repository root\n",
               CORE_FS);
      return 1;
    }

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
