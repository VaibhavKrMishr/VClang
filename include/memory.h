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

// --- Arena Allocator (for AST nodes and custom memory) ---
typedef struct Arena {
    char  *buf;
    size_t used;
    size_t cap;
    struct Arena *prev;
    int   refcount;
} Arena;

Arena *arena_new(size_t initial_cap);
void  *arena_alloc(Arena *a, size_t size);
char  *arena_strdup(Arena *a, const char *s);
void   arena_free(Arena *a);
void   arena_reset(Arena *a);
size_t arena_used(Arena *a);
size_t arena_get_total_allocated(void);
void   arena_reset_total_allocated(void);

// --- Value Lifecycle (runtime values only) ---
typedef struct Value Value;
struct ASTNode;

Value  val_new_int(long x);
Value  val_new_float(double x);
Value  val_new_str(const char *s);
Value  val_new_array(int cap);
void   val_array_push(Value *arr, Value v);
Value  val_new_arena(size_t cap);
struct Env;
typedef struct StructDef StructDef;
Value  val_new_func(const char *name, const char **param_types, const char **params, int param_count, const struct ASTNode *body, struct Env *closure_env);
Value  val_new_struct(const StructDef *def, Arena *custom_arena);
Value  val_new_struct_def(const StructDef *def);
Value  val_zero_for_type(const char *type_name);
Value  val_new_return(Value v);
Value  val_new_err(const char *msg, int line);
Value  val_new_break(void);
Value  val_new_continue(void);
Value  val_copy(Value v);
void   val_clear(Value *v);

// Module Arena Management
void   vclang_register_module_arena(Arena *a);
void   vclang_cleanup_module_arenas(void);

// Printing
void val_print(Value v);
void val_println(Value v);

#endif // VCLANG_MEMORY_H
