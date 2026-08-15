#include <stdlib.h>

size_t vclang_total_mem = 0;

void *vclang_malloc(size_t size) {
  vclang_total_mem += size;
  return malloc(size);
}

void *vclang_calloc(size_t n, size_t size) {
  vclang_total_mem += (n * size);
  return calloc(n, size);
}

void *vclang_realloc(void *ptr, size_t size) {
  vclang_total_mem += size;
  return realloc(ptr, size);
}
