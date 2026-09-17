#ifndef ASH_H
#define ASH_H

#include <stdint.h>
#include <stddef.h>

typedef struct vm vm_t;

typedef intptr_t cell_t;
typedef cell_t *xt_t;
typedef void (*code_t)(vm_t *vm, xt_t xt);

enum {
    F_IMMEDIATE = 1 << 0,
    F_HIDDEN    = 1 << 1
};

/* link | flags name_len name pad | does | code | body
                                     xt[-1] xt[0]  xt+1 */
typedef struct dict_entry {
    struct dict_entry *link;
    uint8_t flags;
    uint8_t name_len;
    char    name[];
} dict_entry_t;

struct vm {
    cell_t *dsp;
    cell_t *rsp;
    xt_t   *ip;

    uint8_t *here;
    dict_entry_t *latest;

    cell_t state;
    cell_t base;
};

/* stack.c */
void   push(vm_t *vm, cell_t x);
cell_t pop(vm_t *vm);
void   rpush(vm_t *vm, cell_t x);
cell_t rpop(vm_t *vm);

/* dict.c */
void          align_here(vm_t *vm);
void         *allot(vm_t *vm, size_t n);
void          comma(vm_t *vm, cell_t x);
dict_entry_t *dict_header(vm_t *vm, const char *name, size_t len);
dict_entry_t *find_word(vm_t *vm, const char *name, size_t len);
xt_t          entry_xt(dict_entry_t *w);

#endif
