#include <string.h>

#include "ash.h"

static uintptr_t align_up(uintptr_t n)
{
    return (n + sizeof(cell_t) - 1) & ~(uintptr_t)(sizeof(cell_t) - 1);
}

void align_here(vm_t *vm)
{
    vm->here = (uint8_t *)align_up((uintptr_t)vm->here);
}

void *allot(vm_t *vm, size_t n)
{
    uint8_t *p = vm->here;
    vm->here += n;
    return p;
}

void comma(vm_t *vm, cell_t x)
{
    *(cell_t *)allot(vm, sizeof(cell_t)) = x;
}

xt_t entry_xt(dict_entry_t *w)
{
    uintptr_t name_end = (uintptr_t)w + offsetof(dict_entry_t, name) + w->name_len;
    cell_t *does = (cell_t *)align_up(name_end);
    return (xt_t)(does + 1);
}

/* Lay down everything up to the code field. The does cell is 0 and the
   code field is left 0: the caller decides the execution strategy. */
dict_entry_t *dict_header(vm_t *vm, const char *name, size_t len)
{
    align_here(vm);
    dict_entry_t *w = allot(vm, offsetof(dict_entry_t, name) + len);
    w->link = vm->latest;
    w->flags = 0;
    w->name_len = (uint8_t)len;
    memcpy(w->name, name, len);
    align_here(vm);
    comma(vm, 0);        /* does */
    comma(vm, 0);        /* code */
    vm->latest = w;
    return w;
}

static int name_eq(const char *a, const char *b, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y)
            return 0;
    }
    return 1;
}

dict_entry_t *find_word(vm_t *vm, const char *name, size_t len)
{
    for (dict_entry_t *w = vm->latest; w; w = w->link) {
        if (w->flags & F_HIDDEN)
            continue;
        if (w->name_len == len && name_eq(w->name, name, len))
            return w;
    }
    return NULL;
}
