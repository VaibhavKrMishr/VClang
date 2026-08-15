#ifndef VCLANG_MEMORY_H
#define VCLANG_MEMORY_H

#include <stdlib.h>

extern size_t vclang_total_mem;

void *vclang_malloc(size_t size);
void *vclang_calloc(size_t n, size_t size);
void *vclang_realloc(void *ptr, size_t size);

#define malloc(s) vclang_malloc(s)
#define calloc(n, s) vclang_calloc(n, s)
#define realloc(p, s) vclang_realloc(p, s)

#endif // VCLANG_MEMORY_H
