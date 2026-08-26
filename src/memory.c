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
  a->refcount = 1;
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
  if (!a) return;
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

void arena_reset(Arena *a) {
  if (!a) return;
  Arena *cur = a->prev;
  while (cur) {
    Arena *prev = cur->prev;
    free(cur->buf);
    free(cur);
    cur = prev;
  }
  a->prev = NULL;
  a->used = 0;
}

size_t arena_used(Arena *a) {
  if (!a) return 0;
  size_t total = a->used;
  Arena *cur = a->prev;
  while (cur) {
    total += cur->used;
    cur = cur->prev;
  }
  return total;
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

Value val_new_array(int cap) {
  if (cap <= 0) cap = 4;
  Value v;
  v.type = VAL_ARRAY;
  v.line = -1;
  v.arr = (ArrayBuffer *)malloc(sizeof(ArrayBuffer));
  v.arr->items = (Value *)malloc(sizeof(Value) * cap);
  v.arr->count = 0;
  v.arr->cap   = cap;
  v.arr->refcount = 1;
  return v;
}

void val_array_push(Value *arr, Value v) {
  if (arr->type != VAL_ARRAY || !arr->arr) return;
  if (arr->arr->count >= arr->arr->cap) {
    arr->arr->cap = (arr->arr->cap == 0) ? 4 : arr->arr->cap * 2;
    arr->arr->items = (Value *)realloc(arr->arr->items, sizeof(Value) * arr->arr->cap);
  }
  arr->arr->items[arr->arr->count++] = val_copy(v);
}

Value val_new_arena(size_t cap) {
  Value v;
  v.type = VAL_ARENA;
  v.line = -1;
  v.arena = arena_new(cap);
  return v;
}

// ============================================================
// Module Arenas registry (to keep AST nodes alive for functions defined in loaded files)
// ============================================================

static Arena **module_arenas = NULL;
static int module_arena_count = 0;
static int module_arena_cap = 0;

void vclang_register_module_arena(Arena *a) {
  if (!a) return;
  if (module_arena_count >= module_arena_cap) {
    module_arena_cap = module_arena_cap == 0 ? 4 : module_arena_cap * 2;
    module_arenas = (Arena **)realloc(module_arenas, sizeof(Arena *) * module_arena_cap);
  }
  module_arenas[module_arena_count++] = a;
}

void vclang_cleanup_module_arenas(void) {
  for (int i = 0; i < module_arena_count; i++) {
    arena_free(module_arenas[i]);
  }
  free(module_arenas);
  module_arenas = NULL;
  module_arena_count = 0;
  module_arena_cap = 0;
}

// ============================================================
// Struct Definition Registry
// ============================================================

static StructDef **struct_defs = NULL;
static int struct_def_count = 0;
static int struct_def_cap = 0;

void struct_def_register(StructDef *def) {
  if (!def) return;
  for (int i = 0; i < struct_def_count; i++) {
    if (strcmp(struct_defs[i]->name, def->name) == 0) {
      struct_defs[i] = def;
      return;
    }
  }
  if (struct_def_count >= struct_def_cap) {
    struct_def_cap = struct_def_cap == 0 ? 8 : struct_def_cap * 2;
    struct_defs = (StructDef **)realloc(struct_defs, sizeof(StructDef *) * struct_def_cap);
  }
  struct_defs[struct_def_count++] = def;
}

const StructDef *struct_def_lookup(const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < struct_def_count; i++) {
    if (strcmp(struct_defs[i]->name, name) == 0) {
      return struct_defs[i];
    }
  }
  return NULL;
}

int struct_def_get_field_index(const StructDef *def, const char *fname) {
  if (!def || !fname) return -1;
  for (int i = 0; i < def->field_count; i++) {
    if (strcmp(def->fields[i].name, fname) == 0) {
      return i;
    }
  }
  return -1;
}

void struct_def_cleanup_all(void) {
  for (int i = 0; i < struct_def_count; i++) {
    StructDef *def = struct_defs[i];
    if (def) {
      free(def->name);
      if (def->fields) {
        for (int j = 0; j < def->field_count; j++) {
          free(def->fields[j].type_name);
          free(def->fields[j].name);
        }
        free(def->fields);
      }
      if (def->methods) {
        free(def->methods);
      }
      free(def);
    }
  }
  free(struct_defs);
  struct_defs = NULL;
  struct_def_count = 0;
  struct_def_cap = 0;
}

Value val_new_func(const char *name, const char **param_types, const char **params, int param_count,
                   const struct ASTNode *body, struct Env *closure_env) {
  (void)closure_env;
  Value v;
  v.type = VAL_FUNC;
  v.line = -1;
  v.func = (Function *)malloc(sizeof(Function));
  v.func->name = vclang_strdup(name);
  v.func->param_count = param_count;
  if (param_count > 0) {
    v.func->params = (char **)malloc(sizeof(char *) * param_count);
    v.func->param_types = (char **)malloc(sizeof(char *) * param_count);
    for (int i = 0; i < param_count; i++) {
      v.func->params[i] = vclang_strdup(params[i]);
      v.func->param_types[i] = (param_types && param_types[i]) ? vclang_strdup(param_types[i]) : NULL;
    }
  } else {
    v.func->params = NULL;
    v.func->param_types = NULL;
  }
  v.func->body = body;
  v.func->closure_env = NULL;
  v.func->refcount = 1;
  return v;
}

Value val_zero_for_type(const char *type_name) {
  if (!type_name) return val_new_int(0);
  if (strcmp(type_name, "int") == 0 || strcmp(type_name, "bool") == 0 ||
      strcmp(type_name, "byte") == 0 || strcmp(type_name, "var") == 0 ||
      strcmp(type_name, "let") == 0) {
    return val_new_int(0);
  }
  if (strcmp(type_name, "float") == 0) {
    return val_new_float(0.0);
  }
  if (strcmp(type_name, "string") == 0) {
    return val_new_str("");
  }
  if (strcmp(type_name, "array") == 0 || strstr(type_name, "[]") != NULL) {
    return val_new_array(0);
  }
  if (strcmp(type_name, "Arena") == 0 || strcmp(type_name, "arena") == 0) {
    return val_new_arena(4096);
  }
  const StructDef *sdef = struct_def_lookup(type_name);
  if (sdef) {
    return val_new_struct(sdef, NULL);
  }
  return val_new_int(0);
}

Value val_new_struct(const StructDef *def, Arena *custom_arena) {
  Value v;
  v.type = VAL_STRUCT;
  v.line = -1;
  if (custom_arena) {
    v.sinst = (StructInstance *)arena_alloc(custom_arena, sizeof(StructInstance));
    v.sinst->def = def;
    v.sinst->refcount = 1;
    v.sinst->arena_owned = 1;
    if (def && def->field_count > 0) {
      v.sinst->fields = (Value *)arena_alloc(custom_arena, sizeof(Value) * def->field_count);
      for (int i = 0; i < def->field_count; i++) {
        v.sinst->fields[i] = val_zero_for_type(def->fields[i].type_name);
      }
    } else {
      v.sinst->fields = NULL;
    }
  } else {
    v.sinst = (StructInstance *)malloc(sizeof(StructInstance));
    v.sinst->def = def;
    v.sinst->refcount = 1;
    v.sinst->arena_owned = 0;
    if (def && def->field_count > 0) {
      v.sinst->fields = (Value *)malloc(sizeof(Value) * def->field_count);
      for (int i = 0; i < def->field_count; i++) {
        v.sinst->fields[i] = val_zero_for_type(def->fields[i].type_name);
      }
    } else {
      v.sinst->fields = NULL;
    }
  }
  return v;
}

Value val_new_struct_def(const StructDef *def) {
  Value v;
  v.type = VAL_STRUCT_DEF;
  v.line = -1;
  v.sdef = def;
  return v;
}

Value val_new_return(Value ret_v) {
  Value v;
  v.type = VAL_RETURN;
  v.line = -1;
  v.ret = (Value *)malloc(sizeof(Value));
  *v.ret = val_copy(ret_v);
  return v;
}

Value val_new_err(const char *msg, int line) {
  Value v;
  v.type = VAL_ERR;
  v.line = line;
  v.err  = vclang_strdup(msg);
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
  if (v.type == VAL_STR) {
    v.str = vclang_strdup(v.str);
  } else if (v.type == VAL_ERR) {
    v.err = vclang_strdup(v.err);
  } else if (v.type == VAL_ARRAY) {
    if (v.arr) v.arr->refcount++;
  } else if (v.type == VAL_FUNC) {
    if (v.func) v.func->refcount++;
  } else if (v.type == VAL_ARENA) {
    if (v.arena) v.arena->refcount++;
  } else if (v.type == VAL_RETURN) {
    if (v.ret) {
      Value *r = (Value *)malloc(sizeof(Value));
      *r = val_copy(*v.ret);
      v.ret = r;
    }
  } else if (v.type == VAL_STRUCT) {
    if (v.sinst) {
      // True pass-by-value semantics: perform independent copy
      const StructDef *def = v.sinst->def;
      Value copy = val_new_struct(def, NULL);
      if (def && def->field_count > 0) {
        for (int i = 0; i < def->field_count; i++) {
          val_clear(&copy.sinst->fields[i]);
          copy.sinst->fields[i] = val_copy(v.sinst->fields[i]);
        }
      }
      return copy;
    }
  }
  return v;
}

void val_clear(Value *v) {
  if (!v) return;
  if (v->type == VAL_INT || v->type == VAL_FLOAT || v->type == VAL_BREAK || v->type == VAL_CONTINUE)
    return;
  if (v->type == VAL_STR) {
    if (v->str) free(v->str);
    v->str = NULL;
  } else if (v->type == VAL_ERR) {
    if (v->err) free(v->err);
    v->err = NULL;
  } else if (v->type == VAL_ARENA) {
    if (v->arena) {
      v->arena->refcount--;
      if (v->arena->refcount <= 0) {
        arena_free(v->arena);
      }
      v->arena = NULL;
    }
  } else if (v->type == VAL_ARRAY) {
    if (v->arr) {
      v->arr->refcount--;
      if (v->arr->refcount <= 0) {
        for (int i = 0; i < v->arr->count; i++) {
          val_clear(&v->arr->items[i]);
        }
        free(v->arr->items);
        free(v->arr);
      }
      v->arr = NULL;
    }
  } else if (v->type == VAL_FUNC) {
    if (v->func) {
      v->func->refcount--;
      if (v->func->refcount <= 0) {
        if (v->func->name) free(v->func->name);
        if (v->func->params) {
          for (int i = 0; i < v->func->param_count; i++) {
            free(v->func->params[i]);
          }
          free(v->func->params);
        }
        if (v->func->param_types) {
          for (int i = 0; i < v->func->param_count; i++) {
            if (v->func->param_types[i]) free(v->func->param_types[i]);
          }
          free(v->func->param_types);
        }
        free(v->func);
      }
      v->func = NULL;
    }
  } else if (v->type == VAL_RETURN) {
    if (v->ret) {
      val_clear(v->ret);
      free(v->ret);
      v->ret = NULL;
    }
  } else if (v->type == VAL_STRUCT) {
    if (v->sinst) {
      if (!v->sinst->arena_owned) {
        v->sinst->refcount--;
        if (v->sinst->refcount <= 0) {
          if (v->sinst->fields && v->sinst->def) {
            for (int i = 0; i < v->sinst->def->field_count; i++) {
              val_clear(&v->sinst->fields[i]);
            }
            free(v->sinst->fields);
          }
          free(v->sinst);
        }
      }
      v->sinst = NULL;
    }
  } else if (v->type == VAL_STRUCT_DEF) {
    v->sdef = NULL;
  }
}

void val_print(Value v) {
  switch (v.type) {
  case VAL_INT:
    printf("%li", v.num);
    break;
  case VAL_FLOAT:
    printf("%g", v.dec);
    break;
  case VAL_STR:
    printf("%s", v.str);
    break;
  case VAL_ARRAY:
    putchar('[');
    if (v.arr) {
      for (int i = 0; i < v.arr->count; i++) {
        if (v.arr->items[i].type == VAL_STR) {
          printf("\"%s\"", v.arr->items[i].str);
        } else {
          val_print(v.arr->items[i]);
        }
        if (i < v.arr->count - 1) printf(", ");
      }
    }
    putchar(']');
    break;
  case VAL_FUNC:
    printf("<function %s(", (v.func && v.func->name) ? v.func->name : "anonymous");
    if (v.func && v.func->params) {
      for (int i = 0; i < v.func->param_count; i++) {
        printf("%s", v.func->params[i]);
        if (i < v.func->param_count - 1) printf(", ");
      }
    }
    printf(")>");
    break;
  case VAL_ARENA:
    printf("<Arena used=%zu cap=%zu>", arena_used(v.arena), v.arena ? v.arena->cap : 0);
    break;
  case VAL_RETURN:
    if (v.ret) val_print(*v.ret);
    break;
  case VAL_STRUCT:
    if (v.sinst && v.sinst->def) {
      printf("%s { ", v.sinst->def->name);
      for (int i = 0; i < v.sinst->def->field_count; i++) {
        printf("%s: ", v.sinst->def->fields[i].name);
        if (v.sinst->fields[i].type == VAL_STR) {
          printf("\"%s\"", v.sinst->fields[i].str);
        } else {
          val_print(v.sinst->fields[i]);
        }
        if (i < v.sinst->def->field_count - 1) printf(", ");
      }
      printf(" }");
    } else {
      printf("<struct null>");
    }
    break;
  case VAL_STRUCT_DEF:
    printf("<type %s>", (v.sdef && v.sdef->name) ? v.sdef->name : "struct");
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


