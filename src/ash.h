#ifndef ASH_H
#define ASH_H

#include <stddef.h>
#include <stdint.h>

typedef struct vm vm_t;

typedef intptr_t cell_t;
typedef cell_t *xt_t;
typedef void (*code_t) (vm_t *vm, xt_t xt);

enum
{
  F_IMMEDIATE = 1 << 0,
  F_HIDDEN = 1 << 1
};

/* link | flags name_len name pad | does | code | body
                                     xt[-1] xt[0]  xt+1 */
typedef struct dict_entry
{
  struct dict_entry *link;
  uint8_t flags;
  uint8_t name_len;
  char name[];
} dict_entry_t;

#define SOURCE_DEPTH 8

typedef struct
{
  const char *buf;
  cell_t len;
  cell_t in;        /* >IN: parse position within buf */
  cell_t source_id; /* 0 = terminal, -1 = string, else fileid */
} input_source_t;

struct vm
{
  cell_t *dsp;
  cell_t *rsp;
  cell_t *dsp0; /* empty-stack values: ABORT/QUIT reset points */
  cell_t *rsp0;
  xt_t *ip;

  uint8_t *here;
  dict_entry_t *latest;

  cell_t state;
  cell_t base;

  input_source_t src[SOURCE_DEPTH];
  cell_t src_depth; /* src[src_depth] is the active source */

  xt_t xt_lit;  /* compiled by literals in compile state */
  xt_t xt_exit; /* compiled by ; */
};

/* stack.c */
void push (vm_t *vm, cell_t x);
cell_t pop (vm_t *vm);
void rpush (vm_t *vm, cell_t x);
cell_t rpop (vm_t *vm);

/* exec.c */
void docol (vm_t *vm, xt_t xt);
void do_exit (vm_t *vm, xt_t xt);
void execute (vm_t *vm, xt_t xt);

/* token.c */
input_source_t *active_source (vm_t *vm);
void push_source (vm_t *vm, const char *buf, cell_t len, cell_t source_id);
void pop_source (vm_t *vm);
const char *next_token (vm_t *vm, size_t *len);

/* interp.c */
int parse_number (vm_t *vm, const char *tok, size_t len, cell_t *out);
void interpret_token (vm_t *vm, const char *tok, size_t len);
void interpret_source (vm_t *vm);

/* prims.c */
void register_prims (vm_t *vm);

/* dict.c */
void align_here (vm_t *vm);
void *allot (vm_t *vm, size_t n);
void comma (vm_t *vm, cell_t x);
dict_entry_t *dict_header (vm_t *vm, const char *name, size_t len);
dict_entry_t *find_word (vm_t *vm, const char *name, size_t len);
xt_t entry_xt (dict_entry_t *w);

#endif
