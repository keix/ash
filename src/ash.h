#ifndef ASH_H
#define ASH_H

#include <stdint.h>
#include <stddef.h>

typedef struct vm vm_t;

typedef intptr_t cell_t;
typedef cell_t *xt_t;
typedef void (*code_t)(vm_t *vm, xt_t xt);

struct vm {
    cell_t *dsp;
    cell_t *rsp;
    xt_t   *ip;

    uint8_t *here;

    cell_t state;
    cell_t base;
};

#endif
