#ifndef VCLANG_MEMORY_H
#define VCLANG_MEMORY_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Tracked Allocation (with NULL checks) ---
extern size_t vclang_total_mem;

void *vclang_malloc(size_t size);
void *vclang_calloc(size_t n, size_t size);
void *vclang_realloc(void *ptr, size_t size);
char *vclang_strdup(const char *s);

// Redirect standard names so all code goes through tracking
#define malloc(s) vclang_malloc(s)
#define calloc(n, s) vclang_calloc(n, s)
#define realloc(p, s) vclang_realloc(p, s)
#define strdup(s) vclang_strdup(s)

// --- Arena Allocator (for AST nodes) ---
typedef struct Arena {
    char  *buf;
    size_t used;
    size_t cap;
    struct Arena *prev;
} Arena;

Arena *arena_new(size_t initial_cap);
void  *arena_alloc(Arena *a, size_t size);
char  *arena_strdup(Arena *a, const char *s);
void   arena_free(Arena *a);
size_t arena_get_total_allocated(void);
void   arena_reset_total_allocated(void);

// --- Value Lifecycle (runtime values only) ---
typedef struct Value Value;

Value  val_new_int(long x);
Value  val_new_float(double x);
Value  val_new_str(const char *s);
Value  val_new_err(const char *msg, int line);
Value  val_new_break(void);
Value  val_new_continue(void);
Value  val_copy(Value v);
void   val_clear(Value *v);

// Printing
void val_print(Value v);
void val_println(Value v);

#endif // VCLANG_MEMORY_H
