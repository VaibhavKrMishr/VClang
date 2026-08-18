// Include our header first to get Arena typedef and function declarations
#include "../include/memory.h"

// Undef our macros so this file can call the real stdlib functions
#undef malloc
#undef calloc
#undef realloc
#undef strdup

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Tracked Allocation
// ============================================================

size_t vclang_total_mem = 0;

static void oom_abort(const char *func, size_t bytes) {
  fprintf(stderr, "VClang: out of memory in %s (requested %zu bytes)\n",
          func, bytes);
  exit(1);
}

void *vclang_malloc(size_t size) {
  void *p = malloc(size);
  if (!p && size > 0) oom_abort("malloc", size);
  vclang_total_mem += size;
  return p;
}

void *vclang_calloc(size_t n, size_t size) {
  void *p = calloc(n, size);
  if (!p && n > 0 && size > 0) oom_abort("calloc", n * size);
  vclang_total_mem += (n * size);
  return p;
}

void *vclang_realloc(void *ptr, size_t size) {
  void *p = realloc(ptr, size);
  if (!p && size > 0) oom_abort("realloc", size);
  // Note: this over-counts because we can't know the old allocation size
  // without a tracking header. It's an approximation.
  vclang_total_mem += size;
  return p;
}

char *vclang_strdup(const char *s) {
  size_t len = strlen(s) + 1;
  char *p = (char *)malloc(len);
  if (!p) oom_abort("strdup", len);
  memcpy(p, s, len);
  return p;
}

// ============================================================
// Arena Allocator
// ============================================================

// Align to 8 bytes for safe struct access
#define ARENA_ALIGN 8
static size_t align_up(size_t n) {
  return (n + ARENA_ALIGN - 1) & ~(ARENA_ALIGN - 1);
}

Arena *arena_new(size_t initial_cap) {
  if (initial_cap == 0) initial_cap = 4096;
  Arena *a = (Arena *)malloc(sizeof(Arena));
  a->buf  = (char *)malloc(initial_cap);
  a->used = 0;
  a->cap  = initial_cap;
  a->prev = NULL;
  return a;
}

// Track total arena bytes for memory reporting (M-4 fix)
static size_t arena_total_bytes = 0;

void *arena_alloc(Arena *a, size_t size) {
  size = align_up(size);
  arena_total_bytes += size;
  if (a->used + size > a->cap) {
    // Current block is full — allocate a new, larger block
    size_t new_cap = a->cap * 2;
    if (new_cap < size) new_cap = size * 2;

    Arena *old = (Arena *)malloc(sizeof(Arena));
    // Move current state into `old` and link it
    *old = *a;

    a->buf  = (char *)malloc(new_cap);
    a->used = 0;
    a->cap  = new_cap;
    a->prev = old;
  }
  void *p = a->buf + a->used;
  a->used += size;
  return p;
}

char *arena_strdup(Arena *a, const char *s) {
  size_t len = strlen(s) + 1;
  char *p = (char *)arena_alloc(a, len);
  memcpy(p, s, len);
  return p;
}

void arena_free(Arena *a) {
  // Walk the linked list of blocks and free them all
  Arena *cur = a->prev;
  free(a->buf);
  while (cur) {
    Arena *prev = cur->prev;
    free(cur->buf);
    free(cur);
    cur = prev;
  }
  // Reset the arena struct itself (caller owns it)
  a->buf  = NULL;
  a->used = 0;
  a->cap  = 0;
  a->prev = NULL;
  free(a);
}

size_t arena_get_total_allocated(void) {
  return arena_total_bytes;
}

void arena_reset_total_allocated(void) {
  arena_total_bytes = 0;
}

// ============================================================
// Value — Runtime value lifecycle
// ============================================================

// We need the full Value definition here.
#include "../include/interpreter.h"

Value val_new_int(long x) {
  Value v;
  v.type = VAL_INT;
  v.line = -1;
  v.num  = x;
  return v;
}

Value val_new_float(double x) {
  Value v;
  v.type = VAL_FLOAT;
  v.line = -1;
  v.dec  = x;
  return v;
}

Value val_new_str(const char *s) {
  Value v;
  v.type = VAL_STR;
  v.line = -1;
  v.str  = vclang_strdup(s);  // M-2 fix: OOM-safe instead of raw strdup
  return v;
}

Value val_new_err(const char *msg, int line) {
  Value v;
  v.type = VAL_ERR;
  v.line = line;
  v.err  = vclang_strdup(msg);  // M-2 fix: OOM-safe instead of raw strdup
  return v;
}

Value val_new_break(void) {
  Value v;
  v.type = VAL_BREAK;
  v.line = -1;
  v.num  = 0;
  return v;
}

Value val_new_continue(void) {
  Value v;
  v.type = VAL_CONTINUE;
  v.line = -1;
  v.num  = 0;
  return v;
}

Value val_copy(Value v) {
  // v is already a struct copy (passed by value).
  // Deep-copy only the heap-owned string data.
  if (v.type == VAL_STR) v.str = vclang_strdup(v.str);
  if (v.type == VAL_ERR) v.err = vclang_strdup(v.err);
  return v;
}

void val_clear(Value *v) {
  if (!v) return;
  // M-3 fix: early return for non-heap types (the common case)
  if (v->type < VAL_STR) return;
  if (v->type == VAL_STR) free(v->str);
  else if (v->type == VAL_ERR) free(v->err);
}

void val_print(Value v) {
  switch (v.type) {
  case VAL_INT:
    printf("%li", v.num);
    break;
  case VAL_FLOAT:
    printf("%lf", v.dec);
    break;
  case VAL_STR:
    printf("%s", v.str);
    break;
  case VAL_ERR:
    if (v.line > 0)
      printf("Error (Line %d): %s", v.line, v.err);
    else
      printf("Error: %s", v.err);
    break;
  case VAL_BREAK:
    printf("break");
    break;
  case VAL_CONTINUE:
    printf("continue");
    break;
  }
}

void val_println(Value v) {
  val_print(v);
  putchar('\n');
  fflush(stdout);
}

